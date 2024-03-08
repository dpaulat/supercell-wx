#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/radar_product_manager_notifier.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <deque>
#include <execution>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <fmt/chrono.h>
#include <qmaplibre.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ =
   "scwx::qt::manager::radar_product_manager";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

typedef std::function<std::shared_ptr<wsr88d::NexradFile>()>
   CreateNexradFileFunction;
typedef std::map<std::chrono::system_clock::time_point,
                 std::weak_ptr<types::RadarProductRecord>>
   RadarProductRecordMap;
typedef std::list<std::shared_ptr<types::RadarProductRecord>>
   RadarProductRecordList;

static constexpr uint32_t NUM_RADIAL_GATES_0_5_DEGREE =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_RADIAL_GATES_1_DEGREE =
   common::MAX_1_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_COORIDNATES_0_5_DEGREE =
   NUM_RADIAL_GATES_0_5_DEGREE * 2;
static constexpr uint32_t NUM_COORIDNATES_1_DEGREE =
   NUM_RADIAL_GATES_1_DEGREE * 2;

static const std::string kDefaultLevel3Product_ {"N0B"};

static constexpr std::chrono::seconds kFastRetryInterval_ {15};
static constexpr std::chrono::seconds kSlowRetryInterval_ {120};

static std::unordered_map<std::string, std::weak_ptr<RadarProductManager>>
                         instanceMap_;
static std::shared_mutex instanceMutex_;

static std::unordered_map<std::string,
                          std::shared_ptr<types::RadarProductRecord>>
                         fileIndex_;
static std::shared_mutex fileIndexMutex_;

static std::mutex fileLoadMutex_;

class ProviderManager : public QObject
{
   Q_OBJECT
public:
   explicit ProviderManager(RadarProductManager*      self,
                            const std::string&        radarId,
                            common::RadarProductGroup group) :
       ProviderManager(self, radarId, group, "???")
   {
   }
   explicit ProviderManager(RadarProductManager*      self,
                            const std::string&        radarId,
                            common::RadarProductGroup group,
                            const std::string&        product) :
       radarId_ {radarId},
       group_ {group},
       product_ {product},
       refreshEnabled_ {false},
       refreshTimer_ {threadPool_},
       refreshTimerMutex_ {},
       provider_ {nullptr}
   {
      connect(this,
              &ProviderManager::NewDataAvailable,
              self,
              &RadarProductManager::NewDataAvailable);
   }
   ~ProviderManager() { threadPool_.join(); };

   std::string name() const;

   void Disable();

   boost::asio::thread_pool threadPool_ {1u};

   const std::string                             radarId_;
   const common::RadarProductGroup               group_;
   const std::string                             product_;
   bool                                          refreshEnabled_;
   boost::asio::steady_timer                     refreshTimer_;
   std::mutex                                    refreshTimerMutex_;
   std::shared_ptr<provider::NexradDataProvider> provider_;

signals:
   void NewDataAvailable(common::RadarProductGroup             group,
                         const std::string&                    product,
                         std::chrono::system_clock::time_point latestTime);
};

class RadarProductManagerImpl
{
public:
   explicit RadarProductManagerImpl(RadarProductManager* self,
                                    const std::string&   radarId) :
       self_ {self},
       radarId_ {radarId},
       initialized_ {false},
       level3ProductsInitialized_ {false},
       radarSite_ {config::RadarSite::Get(radarId)},
       coordinates0_5Degree_ {},
       coordinates1Degree_ {},
       level2ProductRecords_ {},
       level2ProductRecentRecords_ {},
       level3ProductRecordsMap_ {},
       level3ProductRecentRecordsMap_ {},
       level2ProductRecordMutex_ {},
       level3ProductRecordMutex_ {},
       level2ProviderManager_ {std::make_shared<ProviderManager>(
          self_, radarId_, common::RadarProductGroup::Level2)},
       level3ProviderManagerMap_ {},
       level3ProviderManagerMutex_ {},
       initializeMutex_ {},
       level3ProductsInitializeMutex_ {},
       loadLevel2DataMutex_ {},
       loadLevel3DataMutex_ {},
       availableCategoryMap_ {},
       availableCategoryMutex_ {}
   {
      if (radarSite_ == nullptr)
      {
         logger_->warn("Radar site not found: \"{}\"", radarId_);
         radarSite_ = std::make_shared<config::RadarSite>();
      }

      level2ProviderManager_->provider_ =
         provider::NexradDataProviderFactory::CreateLevel2DataProvider(radarId);
   }
   ~RadarProductManagerImpl()
   {
      level2ProviderManager_->Disable();

      std::shared_lock lock(level3ProviderManagerMutex_);
      std::for_each(std::execution::par_unseq,
                    level3ProviderManagerMap_.begin(),
                    level3ProviderManagerMap_.end(),
                    [](auto& p)
                    {
                       auto& [key, providerManager] = p;
                       providerManager->Disable();
                    });

      // Lock other mutexes before destroying, ensure loading is complete
      std::unique_lock loadLevel2DataLock {loadLevel2DataMutex_};
      std::unique_lock loadLevel3DataLock {loadLevel3DataMutex_};

      threadPool_.join();
   }

   RadarProductManager* self_;

   boost::asio::thread_pool threadPool_ {4u};

   std::shared_ptr<ProviderManager>
   GetLevel3ProviderManager(const std::string& product);

   void EnableRefresh(boost::uuids::uuid               uuid,
                      std::shared_ptr<ProviderManager> providerManager,
                      bool                             enabled);
   void RefreshData(std::shared_ptr<ProviderManager> providerManager);

   std::tuple<std::shared_ptr<types::RadarProductRecord>,
              std::chrono::system_clock::time_point>
   GetLevel2ProductRecord(std::chrono::system_clock::time_point time);
   std::tuple<std::shared_ptr<types::RadarProductRecord>,
              std::chrono::system_clock::time_point>
   GetLevel3ProductRecord(const std::string&                    product,
                          std::chrono::system_clock::time_point time);
   std::shared_ptr<types::RadarProductRecord>
   StoreRadarProductRecord(std::shared_ptr<types::RadarProductRecord> record);
   void UpdateRecentRecords(RadarProductRecordList& recentList,
                            std::shared_ptr<types::RadarProductRecord> record);

   void LoadNexradFileAsync(
      CreateNexradFileFunction                           load,
      const std::shared_ptr<request::NexradFileRequest>& request,
      std::mutex&                                        mutex,
      std::chrono::system_clock::time_point              time);
   void
        LoadProviderData(std::chrono::system_clock::time_point time,
                         std::shared_ptr<ProviderManager>      providerManager,
                         RadarProductRecordMap&                recordMap,
                         std::shared_mutex&                    recordMutex,
                         std::mutex&                           loadDataMutex,
                         const std::shared_ptr<request::NexradFileRequest>& request);
   void PopulateLevel2ProductTimes(std::chrono::system_clock::time_point time);
   void PopulateLevel3ProductTimes(const std::string& product,
                                   std::chrono::system_clock::time_point time);

   static void
   PopulateProductTimes(std::shared_ptr<ProviderManager> providerManager,
                        RadarProductRecordMap&           productRecordMap,
                        std::shared_mutex&               productRecordMutex,
                        std::chrono::system_clock::time_point time);

   static void
   LoadNexradFile(CreateNexradFileFunction                           load,
                  const std::shared_ptr<request::NexradFileRequest>& request,
                  std::mutex&                                        mutex,
                  std::chrono::system_clock::time_point              time = {});

   const std::string radarId_;
   bool              initialized_;
   bool              level3ProductsInitialized_;

   std::shared_ptr<config::RadarSite> radarSite_;
   std::size_t                        cacheLimit_ {6u};

   std::vector<float> coordinates0_5Degree_;
   std::vector<float> coordinates1Degree_;

   RadarProductRecordMap  level2ProductRecords_;
   RadarProductRecordList level2ProductRecentRecords_;
   std::unordered_map<std::string, RadarProductRecordMap>
      level3ProductRecordsMap_;
   std::unordered_map<std::string, RadarProductRecordList>
                     level3ProductRecentRecordsMap_;
   std::shared_mutex level2ProductRecordMutex_;
   std::shared_mutex level3ProductRecordMutex_;

   std::shared_ptr<ProviderManager> level2ProviderManager_;
   std::unordered_map<std::string, std::shared_ptr<ProviderManager>>
                     level3ProviderManagerMap_;
   std::shared_mutex level3ProviderManagerMutex_;

   std::mutex initializeMutex_;
   std::mutex level3ProductsInitializeMutex_;
   std::mutex loadLevel2DataMutex_;
   std::mutex loadLevel3DataMutex_;

   common::Level3ProductCategoryMap availableCategoryMap_;
   std::shared_mutex                availableCategoryMutex_;

   std::unordered_map<boost::uuids::uuid,
                      std::shared_ptr<ProviderManager>,
                      boost::hash<boost::uuids::uuid>>
                     refreshMap_ {};
   std::shared_mutex refreshMapMutex_ {};
};

RadarProductManager::RadarProductManager(const std::string& radarId) :
    p(std::make_unique<RadarProductManagerImpl>(this, radarId))
{
}
RadarProductManager::~RadarProductManager() = default;

std::string ProviderManager::name() const
{
   std::string name;

   if (group_ == common::RadarProductGroup::Level3)
   {
      name = fmt::format("{}, {}, {}",
                         radarId_,
                         common::GetRadarProductGroupName(group_),
                         product_);
   }
   else
   {
      name = fmt::format(
         "{}, {}", radarId_, common::GetRadarProductGroupName(group_));
   }

   return name;
}

void ProviderManager::Disable()
{
   logger_->debug("Disabling refresh: {}", name());

   std::unique_lock lock(refreshTimerMutex_);
   refreshEnabled_ = false;
   refreshTimer_.cancel();
}

void RadarProductManager::Cleanup()
{
   {
      std::unique_lock lock(fileIndexMutex_);
      fileIndex_.clear();
   }

   {
      std::unique_lock lock(instanceMutex_);
      instanceMap_.clear();
   }
}

void RadarProductManager::DumpRecords()
{
   scwx::util::async(
      []
      {
         logger_->info("Record Dump");

         std::shared_lock instanceLock {instanceMutex_};
         for (auto& instance : instanceMap_)
         {
            auto radarProductManager = instance.second.lock();
            if (radarProductManager != nullptr)
            {
               logger_->info(" {}", radarProductManager->radar_site()->id());
               logger_->info("  Level 2");

               {
                  std::shared_lock level2ProductLock {
                     radarProductManager->p->level2ProductRecordMutex_};

                  for (auto& record :
                       radarProductManager->p->level2ProductRecords_)
                  {
                     logger_->info("   {}{}",
                                   scwx::util::TimeString(record.first),
                                   record.second.expired() ? " (expired)" : "");
                  }
               }

               logger_->info("  Level 3");

               {
                  std::shared_lock level3ProductLock {
                     radarProductManager->p->level3ProductRecordMutex_};

                  for (auto& recordMap :
                       radarProductManager->p->level3ProductRecordsMap_)
                  {
                     // Product Name
                     logger_->info("   {}", recordMap.first);

                     for (auto& record : recordMap.second)
                     {
                        logger_->info("    {}{}",
                                      scwx::util::TimeString(record.first),
                                      record.second.expired() ? " (expired)" :
                                                                "");
                     }
                  }
               }
            }
         }
      });
}

const std::vector<float>&
RadarProductManager::coordinates(common::RadialSize radialSize) const
{
   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree:
      return p->coordinates0_5Degree_;
   case common::RadialSize::_1Degree:
      return p->coordinates1Degree_;
   default:
      throw std::invalid_argument("Invalid radial size");
   }
}

float RadarProductManager::gate_size() const
{
   return (p->radarSite_->type() == "tdwr") ? 150.0f : 250.0f;
}

std::string RadarProductManager::radar_id() const
{
   return p->radarId_;
}

std::shared_ptr<config::RadarSite> RadarProductManager::radar_site() const
{
   return p->radarSite_;
}

void RadarProductManager::Initialize()
{
   std::unique_lock lock {p->initializeMutex_};

   if (p->initialized_)
   {
      return;
   }

   logger_->debug("Initialize()");

   boost::timer::cpu_timer timer;

   const GeographicLib::Geodesic& geodesic(
      util::GeographicLib::DefaultGeodesic());

   const QMapLibre::Coordinate radar(p->radarSite_->latitude(),
                                     p->radarSite_->longitude());

   const float gateSize = gate_size();

   // Calculate half degree azimuth coordinates
   timer.start();
   std::vector<float>& coordinates0_5Degree = p->coordinates0_5Degree_;

   coordinates0_5Degree.resize(NUM_COORIDNATES_0_5_DEGREE);

   auto radialGates0_5Degree =
      boost::irange<uint32_t>(0, NUM_RADIAL_GATES_0_5_DEGREE);

   std::for_each(
      std::execution::par_unseq,
      radialGates0_5Degree.begin(),
      radialGates0_5Degree.end(),
      [&](uint32_t radialGate)
      {
         const uint16_t gate =
            static_cast<uint16_t>(radialGate % common::MAX_DATA_MOMENT_GATES);
         const uint16_t radial =
            static_cast<uint16_t>(radialGate / common::MAX_DATA_MOMENT_GATES);

         const float  angle  = radial * 0.5f; // 0.5 degree radial
         const float  range  = (gate + 1) * gateSize;
         const size_t offset = radialGate * 2;

         double latitude;
         double longitude;

         geodesic.Direct(
            radar.first, radar.second, angle, range, latitude, longitude);

         coordinates0_5Degree[offset]     = latitude;
         coordinates0_5Degree[offset + 1] = longitude;
      });
   timer.stop();
   logger_->debug("Coordinates (0.5 degree) calculated in {}",
                  timer.format(6, "%ws"));

   // Calculate 1 degree azimuth coordinates
   timer.start();
   std::vector<float>& coordinates1Degree = p->coordinates1Degree_;

   coordinates1Degree.resize(NUM_COORIDNATES_1_DEGREE);

   auto radialGates1Degree =
      boost::irange<uint32_t>(0, NUM_RADIAL_GATES_1_DEGREE);

   std::for_each(
      std::execution::par_unseq,
      radialGates1Degree.begin(),
      radialGates1Degree.end(),
      [&](uint32_t radialGate)
      {
         const uint16_t gate =
            static_cast<uint16_t>(radialGate % common::MAX_DATA_MOMENT_GATES);
         const uint16_t radial =
            static_cast<uint16_t>(radialGate / common::MAX_DATA_MOMENT_GATES);

         const float  angle  = radial * 1.0f; // 1 degree radial
         const float  range  = (gate + 1) * gateSize;
         const size_t offset = radialGate * 2;

         double latitude;
         double longitude;

         geodesic.Direct(
            radar.first, radar.second, angle, range, latitude, longitude);

         coordinates1Degree[offset]     = latitude;
         coordinates1Degree[offset + 1] = longitude;
      });
   timer.stop();
   logger_->debug("Coordinates (1 degree) calculated in {}",
                  timer.format(6, "%ws"));

   p->initialized_ = true;
}

std::shared_ptr<ProviderManager>
RadarProductManagerImpl::GetLevel3ProviderManager(const std::string& product)
{
   std::unique_lock lock(level3ProviderManagerMutex_);

   if (!level3ProviderManagerMap_.contains(product))
   {
      level3ProviderManagerMap_.emplace(
         std::piecewise_construct,
         std::forward_as_tuple(product),
         std::forward_as_tuple(std::make_shared<ProviderManager>(
            self_, radarId_, common::RadarProductGroup::Level3, product)));
      level3ProviderManagerMap_.at(product)->provider_ =
         provider::NexradDataProviderFactory::CreateLevel3DataProvider(radarId_,
                                                                       product);
   }

   std::shared_ptr<ProviderManager> providerManager =
      level3ProviderManagerMap_.at(product);

   return providerManager;
}

void RadarProductManager::EnableRefresh(common::RadarProductGroup group,
                                        const std::string&        product,
                                        bool                      enabled,
                                        boost::uuids::uuid        uuid)
{
   if (group == common::RadarProductGroup::Level2)
   {
      p->EnableRefresh(uuid, p->level2ProviderManager_, enabled);
   }
   else
   {
      std::shared_ptr<ProviderManager> providerManager =
         p->GetLevel3ProviderManager(product);

      // Only enable refresh on available products
      boost::asio::post(
         p->threadPool_,
         [=, this]()
         {
            providerManager->provider_->RequestAvailableProducts();
            auto availableProducts =
               providerManager->provider_->GetAvailableProducts();

            if (std::find(std::execution::par_unseq,
                          availableProducts.cbegin(),
                          availableProducts.cend(),
                          product) != availableProducts.cend())
            {
               p->EnableRefresh(uuid, providerManager, enabled);
            }
         });
   }
}

void RadarProductManagerImpl::EnableRefresh(
   boost::uuids::uuid               uuid,
   std::shared_ptr<ProviderManager> providerManager,
   bool                             enabled)
{
   // Lock the refresh map
   std::unique_lock lock {refreshMapMutex_};

   auto currentProviderManager = refreshMap_.find(uuid);
   if (currentProviderManager != refreshMap_.cend())
   {
      // If the enabling refresh for a different product, or disabling refresh
      if (currentProviderManager->second != providerManager || !enabled)
      {
         // Determine number of entries in the map for the current provider
         // manager
         auto currentProviderManagerCount = std::count_if(
            refreshMap_.cbegin(),
            refreshMap_.cend(),
            [&](const auto& provider)
            { return provider.second == currentProviderManager->second; });

         // If this is the last reference to the provider in the refresh map
         if (currentProviderManagerCount == 1)
         {
            // Disable current provider
            currentProviderManager->second->Disable();
         }

         // Dissociate uuid from current provider manager
         refreshMap_.erase(currentProviderManager);

         // If we are enabling a new provider manager
         if (enabled)
         {
            // Associate uuid to providerManager
            refreshMap_.emplace(uuid, providerManager);
         }
      }
   }
   else if (enabled)
   {
      // We are enabling a new provider manager
      // Associate uuid to provider manager
      refreshMap_.emplace(uuid, providerManager);
   }

   // Release the refresh map mutex
   lock.unlock();

   // We have already handled a disable request by this point. If enabling, and
   // the provider manager refresh isn't already enabled, enable it.
   if (enabled && providerManager->refreshEnabled_ != enabled)
   {
      providerManager->refreshEnabled_ = enabled;

      if (enabled)
      {
         RefreshData(providerManager);
      }
   }
}

void RadarProductManagerImpl::RefreshData(
   std::shared_ptr<ProviderManager> providerManager)
{
   logger_->debug("RefreshData: {}", providerManager->name());

   {
      std::unique_lock lock(providerManager->refreshTimerMutex_);
      providerManager->refreshTimer_.cancel();
   }

   boost::asio::post(
      threadPool_,
      [=, this]()
      {
         using namespace std::chrono_literals;

         auto [newObjects, totalObjects] =
            providerManager->provider_->Refresh();

         std::chrono::milliseconds interval = kFastRetryInterval_;

         if (totalObjects > 0)
         {
            std::string key = providerManager->provider_->FindLatestKey();
            auto        latestTime =
               providerManager->provider_->GetTimePointByKey(key);

            auto updatePeriod = providerManager->provider_->update_period();
            auto lastModified = providerManager->provider_->last_modified();
            auto sinceLastModified =
               std::chrono::system_clock::now() - lastModified;

            // For the default interval, assume products are updated at a
            // constant rate. Expect the next product at a time based on the
            // previous two.
            interval = std::chrono::duration_cast<std::chrono::milliseconds>(
               updatePeriod - sinceLastModified);

            if (updatePeriod > 0s && sinceLastModified > updatePeriod * 5)
            {
               // If it has been at least 5 update periods since the file has
               // been last modified, slow the retry period
               interval = kSlowRetryInterval_;
            }
            else if (interval < std::chrono::milliseconds {kFastRetryInterval_})
            {
               // The interval should be no quicker than the fast retry interval
               interval = kFastRetryInterval_;
            }

            if (newObjects > 0)
            {
               Q_EMIT providerManager->NewDataAvailable(
                  providerManager->group_,
                  providerManager->product_,
                  latestTime);
            }
         }
         else if (providerManager->refreshEnabled_)
         {
            logger_->info("[{}] No data found", providerManager->name());

            // If no data is found, retry at the slow retry interval
            interval = kSlowRetryInterval_;
         }

         if (providerManager->refreshEnabled_)
         {
            std::unique_lock lock(providerManager->refreshTimerMutex_);

            logger_->debug(
               "[{}] Scheduled refresh in {:%M:%S}",
               providerManager->name(),
               std::chrono::duration_cast<std::chrono::seconds>(interval));

            {
               providerManager->refreshTimer_.expires_after(interval);
               providerManager->refreshTimer_.async_wait(
                  [=, this](const boost::system::error_code& e)
                  {
                     if (e == boost::system::errc::success)
                     {
                        RefreshData(providerManager);
                     }
                     else if (e == boost::asio::error::operation_aborted)
                     {
                        logger_->debug("[{}] Data refresh timer cancelled",
                                       providerManager->name());
                     }
                     else
                     {
                        logger_->warn("[{}] Data refresh timer error: {}",
                                      providerManager->name(),
                                      e.message());
                     }
                  });
            }
         }
      });
}

std::set<std::chrono::system_clock::time_point>
RadarProductManager::GetActiveVolumeTimes(
   std::chrono::system_clock::time_point time)
{
   std::unordered_set<std::shared_ptr<provider::NexradDataProvider>>
                                                   providers {};
   std::set<std::chrono::system_clock::time_point> volumeTimes {};
   std::mutex                                      volumeTimesMutex {};

   // Return a default set of volume times if the default time point is given
   if (time == std::chrono::system_clock::time_point {})
   {
      return volumeTimes;
   }

   // Lock the refresh map
   std::shared_lock refreshLock {p->refreshMapMutex_};

   // For each entry in the refresh map (refresh is enabled)
   for (auto& refreshEntry : p->refreshMap_)
   {
      // Add the provider for the current entry
      providers.insert(refreshEntry.second->provider_);
   }

   // Unlock the refresh map
   refreshLock.unlock();

   const auto today     = std::chrono::floor<std::chrono::days>(time);
   const auto yesterday = today - std::chrono::days {1};
   const auto tomorrow  = today + std::chrono::days {1};
   const auto dates     = {yesterday, today, tomorrow};

   // For each provider (in parallel)
   std::for_each(
      std::execution::par_unseq,
      providers.begin(),
      providers.end(),
      [&](const std::shared_ptr<provider::NexradDataProvider>& provider)
      {
         // For yesterday, today and tomorrow (in parallel)
         std::for_each(std::execution::par_unseq,
                       dates.begin(),
                       dates.end(),
                       [&](const auto& date)
                       {
                          // Don't query for a time point in the future
                          if (date > std::chrono::system_clock::now())
                          {
                             return;
                          }

                          // Query the provider for volume time points
                          auto timePoints = provider->GetTimePointsByDate(date);

                          // TODO: Note, this will miss volume times present in
                          // Level 2 products with a second scan

                          // Lock the merged volume time list
                          std::unique_lock volumeTimesLock {volumeTimesMutex};

                          // Copy time points to the merged list
                          std::copy(
                             timePoints.begin(),
                             timePoints.end(),
                             std::inserter(volumeTimes, volumeTimes.end()));
                       });
      });

   // Return merged volume times list
   return volumeTimes;
}

void RadarProductManagerImpl::LoadProviderData(
   std::chrono::system_clock::time_point              time,
   std::shared_ptr<ProviderManager>                   providerManager,
   RadarProductRecordMap&                             recordMap,
   std::shared_mutex&                                 recordMutex,
   std::mutex&                                        loadDataMutex,
   const std::shared_ptr<request::NexradFileRequest>& request)
{
   logger_->debug("LoadProviderData: {}, {}",
                  providerManager->name(),
                  scwx::util::TimeString(time));

   LoadNexradFileAsync(
      [=, &recordMap, &recordMutex]() -> std::shared_ptr<wsr88d::NexradFile>
      {
         std::shared_ptr<types::RadarProductRecord> existingRecord = nullptr;
         std::shared_ptr<wsr88d::NexradFile>        nexradFile     = nullptr;

         {
            std::shared_lock sharedLock {recordMutex};

            auto it = recordMap.find(time);
            if (it != recordMap.cend())
            {
               existingRecord = it->second.lock();

               if (existingRecord != nullptr)
               {
                  logger_->debug(
                     "Data previously loaded, loading from data cache");
               }
            }
         }

         if (existingRecord == nullptr)
         {
            std::string key = providerManager->provider_->FindKey(time);

            if (!key.empty())
            {
               nexradFile = providerManager->provider_->LoadObjectByKey(key);
            }
            else
            {
               logger_->warn("Attempting to load object without key: {}",
                             scwx::util::TimeString(time));
            }
         }
         else
         {
            nexradFile = existingRecord->nexrad_file();
         }

         return nexradFile;
      },
      request,
      loadDataMutex,
      time);
}

void RadarProductManager::LoadLevel2Data(
   std::chrono::system_clock::time_point              time,
   const std::shared_ptr<request::NexradFileRequest>& request)
{
   logger_->debug("LoadLevel2Data: {}", scwx::util::TimeString(time));

   p->LoadProviderData(time,
                       p->level2ProviderManager_,
                       p->level2ProductRecords_,
                       p->level2ProductRecordMutex_,
                       p->loadLevel2DataMutex_,
                       request);
}

void RadarProductManager::LoadLevel3Data(
   const std::string&                                 product,
   std::chrono::system_clock::time_point              time,
   const std::shared_ptr<request::NexradFileRequest>& request)
{
   logger_->debug("LoadLevel3Data: {}", scwx::util::TimeString(time));

   // Look up provider manager
   std::shared_lock providerManagerLock(p->level3ProviderManagerMutex_);
   auto level3ProviderManager = p->level3ProviderManagerMap_.find(product);
   if (level3ProviderManager == p->level3ProviderManagerMap_.cend())
   {
      logger_->debug("No level 3 provider manager for product: {}", product);
      return;
   }
   providerManagerLock.unlock();

   // Look up product record
   std::unique_lock       productRecordLock(p->level3ProductRecordMutex_);
   RadarProductRecordMap& level3ProductRecords =
      p->level3ProductRecordsMap_[product];
   productRecordLock.unlock();

   // Load provider data
   p->LoadProviderData(time,
                       level3ProviderManager->second,
                       level3ProductRecords,
                       p->level3ProductRecordMutex_,
                       p->loadLevel3DataMutex_,
                       request);
}

void RadarProductManager::LoadData(
   std::istream& is, const std::shared_ptr<request::NexradFileRequest>& request)
{
   logger_->debug("LoadData()");

   scwx::util::async(
      [=, &is]()
      {
         RadarProductManagerImpl::LoadNexradFile(
            [=, &is]() -> std::shared_ptr<wsr88d::NexradFile>
            { return wsr88d::NexradFileFactory::Create(is); },
            request,
            fileLoadMutex_);
      });
}

void RadarProductManager::LoadFile(
   const std::string&                                 filename,
   const std::shared_ptr<request::NexradFileRequest>& request)
{
   logger_->debug("LoadFile: {}", filename);

   std::shared_ptr<types::RadarProductRecord> existingRecord = nullptr;

   {
      std::shared_lock lock {fileIndexMutex_};
      auto             it = fileIndex_.find(filename);
      if (it != fileIndex_.cend())
      {
         logger_->debug("File previously loaded, loading from file cache");

         existingRecord = it->second;
      }
   }

   if (existingRecord == nullptr)
   {
      QObject::connect(request.get(),
                       &request::NexradFileRequest::RequestComplete,
                       [=](std::shared_ptr<request::NexradFileRequest> request)
                       {
                          auto record = request->radar_product_record();

                          if (record != nullptr)
                          {
                             std::unique_lock lock {fileIndexMutex_};
                             fileIndex_[filename] = record;
                          }
                       });

      scwx::util::async(
         [=]()
         {
            RadarProductManagerImpl::LoadNexradFile(
               [=]() -> std::shared_ptr<wsr88d::NexradFile>
               { return wsr88d::NexradFileFactory::Create(filename); },
               request,
               fileLoadMutex_);
         });
   }
   else if (request != nullptr)
   {
      request->set_radar_product_record(existingRecord);
      Q_EMIT request->RequestComplete(request);
   }
}

void RadarProductManagerImpl::LoadNexradFileAsync(
   CreateNexradFileFunction                           load,
   const std::shared_ptr<request::NexradFileRequest>& request,
   std::mutex&                                        mutex,
   std::chrono::system_clock::time_point              time)
{
   boost::asio::post(threadPool_,
                     [=, &mutex]()
                     { LoadNexradFile(load, request, mutex, time); });
}

void RadarProductManagerImpl::LoadNexradFile(
   CreateNexradFileFunction                           load,
   const std::shared_ptr<request::NexradFileRequest>& request,
   std::mutex&                                        mutex,
   std::chrono::system_clock::time_point              time)
{
   std::unique_lock lock {mutex};

   std::shared_ptr<wsr88d::NexradFile> nexradFile = load();

   std::shared_ptr<types::RadarProductRecord> record = nullptr;

   bool fileValid = (nexradFile != nullptr);

   if (fileValid)
   {
      record = types::RadarProductRecord::Create(nexradFile);

      // If the time is already determined, override the time in the file.
      // Sometimes, level 2 data has been seen to be a few seconds off
      // between filename and file data. Overriding this can help prevent
      // issues with locating and storing the correct records.
      if (time != std::chrono::system_clock::time_point {})
      {
         record->set_time(time);
      }

      std::string recordRadarId = (record->radar_id());
      if (recordRadarId.empty())
      {
         recordRadarId = request->current_radar_site();
      }

      std::shared_ptr<RadarProductManager> manager =
         RadarProductManager::Instance(recordRadarId);

      manager->Initialize();
      record = manager->p->StoreRadarProductRecord(record);
   }

   lock.unlock();

   if (request != nullptr)
   {
      request->set_radar_product_record(record);
      Q_EMIT request->RequestComplete(request);
   }
}

void RadarProductManagerImpl::PopulateLevel2ProductTimes(
   std::chrono::system_clock::time_point time)
{
   PopulateProductTimes(level2ProviderManager_,
                        level2ProductRecords_,
                        level2ProductRecordMutex_,
                        time);
}

void RadarProductManagerImpl::PopulateLevel3ProductTimes(
   const std::string& product, std::chrono::system_clock::time_point time)
{
   // Get provider manager
   auto level3ProviderManager = GetLevel3ProviderManager(product);

   // Get product records
   std::unique_lock level3ProductRecordLock {level3ProductRecordMutex_};
   auto&            level3ProductRecords = level3ProductRecordsMap_[product];
   level3ProductRecordLock.unlock();

   PopulateProductTimes(level3ProviderManager,
                        level3ProductRecords,
                        level3ProductRecordMutex_,
                        time);
}

void RadarProductManagerImpl::PopulateProductTimes(
   std::shared_ptr<ProviderManager>      providerManager,
   RadarProductRecordMap&                productRecordMap,
   std::shared_mutex&                    productRecordMutex,
   std::chrono::system_clock::time_point time)
{
   const auto today = std::chrono::floor<std::chrono::days>(time);

   // Don't query for the epoch
   if (today == std::chrono::system_clock::time_point {})
   {
      return;
   }

   const auto yesterday = today - std::chrono::days {1};
   const auto tomorrow  = today + std::chrono::days {1};
   const auto dates     = {yesterday, today, tomorrow};

   std::set<std::chrono::system_clock::time_point> volumeTimes {};
   std::mutex                                      volumeTimesMutex {};

   // For yesterday, today and tomorrow (in parallel)
   std::for_each(std::execution::par_unseq,
                 dates.begin(),
                 dates.end(),
                 [&](const auto& date)
                 {
                    // Don't query for a time point in the future
                    if (date > std::chrono::system_clock::now())
                    {
                       return;
                    }

                    // Query the provider for volume time points
                    auto timePoints =
                       providerManager->provider_->GetTimePointsByDate(date);

                    // Lock the merged volume time list
                    std::unique_lock volumeTimesLock {volumeTimesMutex};

                    // Copy time points to the merged list
                    std::copy(timePoints.begin(),
                              timePoints.end(),
                              std::inserter(volumeTimes, volumeTimes.end()));
                 });

   // Lock the product record map
   std::unique_lock lock {productRecordMutex};

   // Merge volume times into map
   std::transform(volumeTimes.cbegin(),
                  volumeTimes.cend(),
                  std::inserter(productRecordMap, productRecordMap.begin()),
                  [](const std::chrono::system_clock::time_point& time)
                  {
                     return std::pair<std::chrono::system_clock::time_point,
                                      std::weak_ptr<types::RadarProductRecord>>(
                        time, std::weak_ptr<types::RadarProductRecord> {});
                  });
}

std::tuple<std::shared_ptr<types::RadarProductRecord>,
           std::chrono::system_clock::time_point>
RadarProductManagerImpl::GetLevel2ProductRecord(
   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<types::RadarProductRecord> record {nullptr};
   RadarProductRecordMap::const_pointer       recordPtr {nullptr};
   std::chrono::system_clock::time_point      recordTime {time};

   // Ensure Level 2 product records are updated
   PopulateLevel2ProductTimes(time);

   if (!level2ProductRecords_.empty() &&
       time == std::chrono::system_clock::time_point {})
   {
      // If a default-initialized time point is given, return the latest record
      recordPtr = &(*level2ProductRecords_.rbegin());
   }
   else
   {
      recordPtr =
         scwx::util::GetBoundedElementPointer(level2ProductRecords_, time);
   }

   if (recordPtr != nullptr)
   {
      // Don't check for an exact time match for level 2 products
      recordTime = recordPtr->first;
      record     = recordPtr->second.lock();
   }

   if (recordPtr != nullptr && record == nullptr &&
       recordTime != std::chrono::system_clock::time_point {})
   {
      // Product is expired, reload it
      std::shared_ptr<request::NexradFileRequest> request =
         std::make_shared<request::NexradFileRequest>(radarId_);

      QObject::connect(
         request.get(),
         &request::NexradFileRequest::RequestComplete,
         self_,
         [this](std::shared_ptr<request::NexradFileRequest> request)
         {
            if (request->radar_product_record() != nullptr)
            {
               Q_EMIT self_->DataReloaded(request->radar_product_record());
            }
         });

      self_->LoadLevel2Data(recordTime, request);
   }

   return {record, recordTime};
}

std::tuple<std::shared_ptr<types::RadarProductRecord>,
           std::chrono::system_clock::time_point>
RadarProductManagerImpl::GetLevel3ProductRecord(
   const std::string& product, std::chrono::system_clock::time_point time)
{
   std::shared_ptr<types::RadarProductRecord> record {nullptr};
   RadarProductRecordMap::const_pointer       recordPtr {nullptr};
   std::chrono::system_clock::time_point      recordTime {time};

   // Ensure Level 3 product records are updated
   PopulateLevel3ProductTimes(product, time);

   std::unique_lock lock {level3ProductRecordMutex_};

   auto it = level3ProductRecordsMap_.find(product);

   if (it != level3ProductRecordsMap_.cend() && !it->second.empty())
   {
      if (time == std::chrono::system_clock::time_point {})
      {
         // If a default-initialized time point is given, return the latest
         // record
         recordPtr = &(*it->second.rbegin());
      }
      else
      {
         recordPtr = scwx::util::GetBoundedElementPointer(it->second, time);
      }
   }

   // Lock is no longer needed
   lock.unlock();

   if (recordPtr != nullptr)
   {
      // Don't check for an exact time match for level 3 products
      recordTime = recordPtr->first;
      record     = recordPtr->second.lock();
   }

   if (recordPtr != nullptr && record == nullptr &&
       recordTime != std::chrono::system_clock::time_point {})
   {
      // Product is expired, reload it
      std::shared_ptr<request::NexradFileRequest> request =
         std::make_shared<request::NexradFileRequest>(radarId_);

      QObject::connect(
         request.get(),
         &request::NexradFileRequest::RequestComplete,
         self_,
         [this](std::shared_ptr<request::NexradFileRequest> request)
         {
            if (request->radar_product_record() != nullptr)
            {
               Q_EMIT self_->DataReloaded(request->radar_product_record());
            }
         });

      self_->LoadLevel3Data(product, recordTime, request);
   }

   return {record, recordTime};
}

std::shared_ptr<types::RadarProductRecord>
RadarProductManagerImpl::StoreRadarProductRecord(
   std::shared_ptr<types::RadarProductRecord> record)
{
   logger_->debug("StoreRadarProductRecord()");

   std::shared_ptr<types::RadarProductRecord> storedRecord = nullptr;

   auto timeInSeconds =
      std::chrono::time_point_cast<std::chrono::seconds,
                                   std::chrono::system_clock>(record->time());

   if (record->radar_product_group() == common::RadarProductGroup::Level2)
   {
      std::unique_lock lock {level2ProductRecordMutex_};

      auto it = level2ProductRecords_.find(timeInSeconds);
      if (it != level2ProductRecords_.cend())
      {
         storedRecord = it->second.lock();

         if (storedRecord != nullptr)
         {
            logger_->debug(
               "Level 2 product previously loaded, loading from cache");
         }
      }

      if (storedRecord == nullptr)
      {
         storedRecord                         = record;
         level2ProductRecords_[timeInSeconds] = record;
      }

      UpdateRecentRecords(level2ProductRecentRecords_, storedRecord);
   }
   else if (record->radar_product_group() == common::RadarProductGroup::Level3)
   {
      std::unique_lock lock {level3ProductRecordMutex_};

      auto& productMap = level3ProductRecordsMap_[record->radar_product()];

      auto it = productMap.find(timeInSeconds);
      if (it != productMap.cend())
      {
         storedRecord = it->second.lock();

         if (storedRecord != nullptr)
         {
            logger_->debug(
               "Level 3 product previously loaded, loading from cache");
         }
      }

      if (storedRecord == nullptr)
      {
         storedRecord              = record;
         productMap[timeInSeconds] = record;
      }

      UpdateRecentRecords(
         level3ProductRecentRecordsMap_[record->radar_product()], storedRecord);
   }

   return storedRecord;
}

void RadarProductManagerImpl::UpdateRecentRecords(
   RadarProductRecordList&                    recentList,
   std::shared_ptr<types::RadarProductRecord> record)
{
   const std::size_t recentListMaxSize {cacheLimit_};
   bool              iteratorErased = false;

   auto it = std::find(recentList.cbegin(), recentList.cend(), record);
   if (it != recentList.cbegin() && it != recentList.cend())
   {
      // If the record exists beyond the front of the list, remove it
      recentList.erase(it);
      iteratorErased = true;
   }

   if (iteratorErased || recentList.size() == 0 || it != recentList.cbegin())
   {
      // Add the record to the front of the list, unless it's already there
      recentList.push_front(record);
   }

   while (recentList.size() > recentListMaxSize)
   {
      // Remove from the end of the list while it's too big
      recentList.pop_back();
   }
}

std::tuple<std::shared_ptr<wsr88d::rda::ElevationScan>,
           float,
           std::vector<float>,
           std::chrono::system_clock::time_point>
RadarProductManager::GetLevel2Data(wsr88d::rda::DataBlockType dataBlockType,
                                   float                      elevation,
                                   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<wsr88d::rda::ElevationScan> radarData    = nullptr;
   float                                       elevationCut = 0.0f;
   std::vector<float>                          elevationCuts;

   std::shared_ptr<types::RadarProductRecord> record;
   std::tie(record, time) = p->GetLevel2ProductRecord(time);

   if (record != nullptr)
   {
      std::tie(radarData, elevationCut, elevationCuts) =
         record->level2_file()->GetElevationScan(
            dataBlockType, elevation, time);
   }

   return {radarData, elevationCut, elevationCuts, time};
}

std::tuple<std::shared_ptr<wsr88d::rpg::Level3Message>,
           std::chrono::system_clock::time_point>
RadarProductManager::GetLevel3Data(const std::string& product,
                                   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<wsr88d::rpg::Level3Message> message = nullptr;

   std::shared_ptr<types::RadarProductRecord> record;
   std::tie(record, time) = p->GetLevel3ProductRecord(product, time);

   if (record != nullptr)
   {
      message = record->level3_file()->message();
   }

   return {message, time};
}

common::Level3ProductCategoryMap
RadarProductManager::GetAvailableLevel3Categories()
{
   std::shared_lock lock {p->availableCategoryMutex_};

   return p->availableCategoryMap_;
}

std::vector<std::string> RadarProductManager::GetLevel3Products()
{
   auto level3ProviderManager =
      p->GetLevel3ProviderManager(kDefaultLevel3Product_);
   return level3ProviderManager->provider_->GetAvailableProducts();
}

void RadarProductManager::SetCacheLimit(size_t cacheLimit)
{
   p->cacheLimit_ = cacheLimit;
}

void RadarProductManager::UpdateAvailableProducts()
{
   std::lock_guard<std::mutex> guard(p->level3ProductsInitializeMutex_);

   if (p->level3ProductsInitialized_)
   {
      return;
   }

   // Although not complete here, only initialize once. Signal will be emitted
   // once complete.
   p->level3ProductsInitialized_ = true;

   logger_->debug("UpdateAvailableProducts()");

   boost::asio::post(
      p->threadPool_,
      [this]()
      {
         auto level3ProviderManager =
            p->GetLevel3ProviderManager(kDefaultLevel3Product_);
         level3ProviderManager->provider_->RequestAvailableProducts();
         auto updatedAwipsIdList =
            level3ProviderManager->provider_->GetAvailableProducts();

         std::unique_lock lock {p->availableCategoryMutex_};

         for (common::Level3ProductCategory category :
              common::Level3ProductCategoryIterator())
         {
            const auto& products =
               common::GetLevel3ProductsByCategory(category);

            std::unordered_map<std::string, std::vector<std::string>>
               availableProducts;

            for (const auto& product : products)
            {
               const auto& awipsIds =
                  common::GetLevel3AwipsIdsByProduct(product);

               std::vector<std::string> availableAwipsIds;

               for (const auto& awipsId : awipsIds)
               {
                  if (std::find(updatedAwipsIdList.cbegin(),
                                updatedAwipsIdList.cend(),
                                awipsId) != updatedAwipsIdList.cend())
                  {
                     availableAwipsIds.push_back(awipsId);
                  }
               }

               if (!availableAwipsIds.empty())
               {
                  availableProducts.insert_or_assign(
                     product, std::move(availableAwipsIds));
               }
            }

            if (!availableProducts.empty())
            {
               p->availableCategoryMap_.insert_or_assign(
                  category, std::move(availableProducts));
            }
            else
            {
               p->availableCategoryMap_.erase(category);
            }
         }

         Q_EMIT Level3ProductsChanged();
      });
}

std::shared_ptr<RadarProductManager>
RadarProductManager::Instance(const std::string& radarSite)
{
   std::shared_ptr<RadarProductManager> instance        = nullptr;
   bool                                 instanceCreated = false;

   {
      std::unique_lock lock {instanceMutex_};

      // Look up instance weak pointer
      auto it = instanceMap_.find(radarSite);
      if (it != instanceMap_.end())
      {
         // Attempt to convert the weak pointer to a shared pointer. It may have
         // been garbage collected.
         instance = it->second.lock();
      }

      // If no active instance was found, create a new one
      if (instance == nullptr)
      {
         instance = std::make_shared<RadarProductManager>(radarSite);
         instanceMap_.insert_or_assign(radarSite, instance);
         instanceCreated = true;
      }
   }

   if (instanceCreated)
   {
      Q_EMIT RadarProductManagerNotifier::Instance().RadarProductManagerCreated(
         radarSite);
   }

   return instance;
}

#include "radar_product_manager.moc"

} // namespace manager
} // namespace qt
} // namespace scwx
