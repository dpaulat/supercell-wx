#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/provider_manager.hpp>
#include <scwx/qt/manager/radar_coordinate_table.hpp>
#include <scwx/qt/manager/radar_product_manager_notifier.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/time_types.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/provider/aws_level2_chunks_data_provider.hpp>
#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <execution>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/container_hash/hash.hpp>
#include <fmt/chrono.h>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::qt::manager
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

static const std::string kDefaultLevel3Product_ {"N0B"};

static std::unordered_map<std::string, std::weak_ptr<RadarProductManager>>
                         instanceMap_;
static std::shared_mutex instanceMutex_;

static std::unordered_map<std::string,
                          std::shared_ptr<types::RadarProductRecord>>
                         fileIndex_;
static std::shared_mutex fileIndexMutex_;

static std::mutex fileLoadMutex_;

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
       level2ProviderManager_ {std::make_shared<ProviderManager>(
          self_, radarId_, common::RadarProductGroup::Level2)},
       level2ChunksProviderManager_ {std::make_shared<ProviderManager>(
          self_, radarId_, common::RadarProductGroup::Level2, "???", true)}
   {
      if (radarSite_ == nullptr)
      {
         logger_->warn("Radar site not found: \"{}\"", radarId_);
         radarSite_ = std::make_shared<config::RadarSite>();
      }

      level2ProviderManager_->set_provider(
         provider::NexradDataProviderFactory::CreateLevel2DataProvider(
            radarId));
      level2ChunksProviderManager_->set_provider(
         provider::NexradDataProviderFactory::CreateLevel2ChunksDataProvider(
            radarId));

      auto level2ChunksProvider =
         std::dynamic_pointer_cast<provider::AwsLevel2ChunksDataProvider>(
            level2ChunksProviderManager_->provider());
      if (level2ChunksProvider != nullptr)
      {
         level2ChunksProvider->SetLevel2DataProvider(
            std::dynamic_pointer_cast<provider::AwsLevel2DataProvider>(
               level2ProviderManager_->provider()));
      }

      // Match RadarProductManager::gate_size(); cannot call self_->gate_size()
      // here because RadarProductManager::p is not constructed yet.
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
      const float gateSize = (radarSite_->type() == "tdwr") ? 150.0f : 250.0f;
      coordinateTable_     = std::make_unique<RadarCoordinateTable>(
         radarSite_->latitude(), radarSite_->longitude(), gateSize);
   }
   ~RadarProductManagerImpl()
   {
      const bool shutdown = true;

      level2ProviderManager_->Disable(shutdown);
      level2ChunksProviderManager_->Disable(shutdown);

      std::shared_lock lock(level3ProviderManagerMutex_);
      std::for_each(std::execution::par,
                    level3ProviderManagerMap_.begin(),
                    level3ProviderManagerMap_.end(),
                    [](auto& p)
                    {
                       auto& [key, providerManager] = p;
                       providerManager->Disable(shutdown);
                    });
      lock.unlock();

      threadPool_.stop();
      threadPool_.join();
   }

   RadarProductManager* self_;

   boost::asio::thread_pool threadPool_ {4u};

   std::shared_ptr<ProviderManager>
   GetLevel3ProviderManager(const std::string& product);

   void EnableRefresh(
      boost::uuids::uuid                                uuid,
      const std::set<std::shared_ptr<ProviderManager>>& providerManagers,
      bool                                              enabled);

   std::tuple<std::map<std::chrono::system_clock::time_point,
                       std::shared_ptr<types::RadarProductRecord>>,
              types::RadarProductLoadStatus>
   GetLevel2ProductRecords(std::chrono::system_clock::time_point time);
   std::tuple<std::shared_ptr<types::RadarProductRecord>,
              std::chrono::system_clock::time_point,
              types::RadarProductLoadStatus>
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

   bool AreLevel2ProductTimesPopulated(
      std::chrono::system_clock::time_point time) const;
   bool
   AreLevel3ProductTimesPopulated(const std::string&                    product,
                                  std::chrono::system_clock::time_point time);

   void PopulateLevel2ProductTimes(std::chrono::system_clock::time_point time,
                                   bool update = true);
   void PopulateLevel3ProductTimes(const std::string& product,
                                   std::chrono::system_clock::time_point time,
                                   bool update = true);

   void UpdateAvailableProductsSync();

   static bool AreProductTimesPopulated(
      const std::shared_ptr<ProviderManager>& providerManager,
      std::chrono::system_clock::time_point   time);

   static void
   PopulateProductTimes(std::shared_ptr<ProviderManager> providerManager,
                        RadarProductRecordMap&           productRecordMap,
                        std::shared_mutex&               productRecordMutex,
                        std::chrono::system_clock::time_point time,
                        bool                                  update);

   static void
   LoadNexradFile(CreateNexradFileFunction                           load,
                  const std::shared_ptr<request::NexradFileRequest>& request,
                  std::mutex&                                        mutex,
                  std::chrono::system_clock::time_point              time = {});

   const std::string radarId_;
   bool              initialized_;
   bool              level3ProductsInitialized_;
   bool              level3AvailabilityReady_ {false};

   std::shared_ptr<config::RadarSite> radarSite_;
   std::size_t                        cacheLimit_ {6u};

   std::unique_ptr<RadarCoordinateTable> coordinateTable_ {};

   RadarProductRecordMap  level2ProductRecords_ {};
   RadarProductRecordList level2ProductRecentRecords_ {};
   std::unordered_map<std::string, RadarProductRecordMap>
      level3ProductRecordsMap_ {};
   std::unordered_map<std::string, RadarProductRecordList>
                     level3ProductRecentRecordsMap_ {};
   std::shared_mutex level2ProductRecordMutex_ {};
   std::shared_mutex level3ProductRecordMutex_ {};

   std::shared_ptr<ProviderManager> level2ProviderManager_;
   std::shared_ptr<ProviderManager> level2ChunksProviderManager_;
   std::unordered_map<std::string, std::shared_ptr<ProviderManager>>
                     level3ProviderManagerMap_ {};
   std::shared_mutex level3ProviderManagerMutex_ {};

   std::mutex initializeMutex_ {};
   std::mutex level3ProductsInitializeMutex_ {};
   std::mutex loadLevel2DataMutex_ {};
   std::mutex loadLevel3DataMutex_ {};

   common::Level3ProductCategoryMap availableCategoryMap_ {};
   std::shared_mutex                availableCategoryMutex_ {};

   std::optional<float> incomingLevel2Elevation_ {};

   std::unordered_map<boost::uuids::uuid,
                      std::set<std::shared_ptr<ProviderManager>>,
                      boost::hash<boost::uuids::uuid>>
                     refreshMap_ {};
   std::shared_mutex refreshMapMutex_ {};
};

RadarProductManager::RadarProductManager(const std::string& radarId) :
    p(std::make_unique<RadarProductManagerImpl>(this, radarId))
{
}
RadarProductManager::~RadarProductManager() = default;

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

// Cached lat/lon grid; first use for a (radialSize, smoothing) pair may block
// on lazy initialization inside RadarCoordinateTable.
const std::vector<float>&
RadarProductManager::coordinates(common::RadialSize radialSize,
                                 bool               smoothingEnabled) const
{
   return p->coordinateTable_->coordinates(radialSize, smoothingEnabled);
}
const scwx::util::time_zone* RadarProductManager::default_time_zone() const
{
   types::DefaultTimeZone defaultTimeZone = types::GetDefaultTimeZone(
      settings::GeneralSettings::Instance().default_time_zone().GetValue());

   switch (defaultTimeZone)
   {
   case types::DefaultTimeZone::Radar:
   {
      auto radarSite = radar_site();
      if (radarSite != nullptr)
      {
         return radarSite->time_zone();
      }
      [[fallthrough]];
   }

   case types::DefaultTimeZone::Local:
#if (__cpp_lib_chrono >= 201907L)
      return std::chrono::current_zone();
#else
      return date::current_zone();
#endif

   default:
      return nullptr;
   }
}

types::RadarType RadarProductManager::radar_type() const
{
   return p->radarSite_->type();
}

float RadarProductManager::gate_size() const
{
   // wsr88d is 250 meters per gate, others are 150 meters per gate
   switch (radar_type())
   {
   case types::RadarType::WSR88D:
      return 250.0f; // NOLINT(cppcoreguidelines-avoid-magic-numbers)

   case types::RadarType::Research:
   case types::RadarType::FAA:
   case types::RadarType::TDWR:
   case types::RadarType::Unknown:
   default:
      return 150.0f; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
   }
}

std::optional<float> RadarProductManager::incoming_level_2_elevation() const
{
   return p->incomingLevel2Elevation_;
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

   // Lat/lon tables still come from coordinates() on demand; Initialize() does
   // not resize or fill those vectors.

   if (radar_type() != types::RadarType::WSR88D)
   {
      p->initialized_ = true;
      return;
   }

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
      level3ProviderManagerMap_.at(product)->set_provider(
         provider::NexradDataProviderFactory::CreateLevel3DataProvider(
            radarId_, product));
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
      p->EnableRefresh(
         uuid,
         {p->level2ProviderManager_, p->level2ChunksProviderManager_},
         enabled);
   }
   else
   {
      const std::shared_ptr<ProviderManager> providerManager =
         p->GetLevel3ProviderManager(product);

      // Only enable refresh on available products
      if (enabled)
      {
         boost::asio::post(
            p->threadPool_,
            [providerManager, product, uuid, enabled, this]()
            {
               try
               {
                  providerManager->provider()->RequestAvailableProducts();
                  const auto availableProducts =
                     providerManager->provider()->GetAvailableProducts();

                  if (std::find(std::execution::par,
                                availableProducts.cbegin(),
                                availableProducts.cend(),
                                product) != availableProducts.cend())
                  {
                     p->EnableRefresh(uuid, {providerManager}, enabled);
                  }
               }
               catch (const std::exception& ex)
               {
                  logger_->error(ex.what());
               }
            });
      }
      else
      {
         p->EnableRefresh(uuid, {providerManager}, enabled);
      }
   }
}

void RadarProductManagerImpl::EnableRefresh(
   boost::uuids::uuid                                uuid,
   const std::set<std::shared_ptr<ProviderManager>>& providerManagers,
   bool                                              enabled)
{
   // Lock the refresh map
   std::unique_lock lock {refreshMapMutex_};

   auto currentProviderManagers = refreshMap_.find(uuid);
   if (currentProviderManagers != refreshMap_.cend())
   {
      for (const auto& currentProviderManager : currentProviderManagers->second)
      {
         currentProviderManager->decrement_refresh_count();
         // If the enabling refresh for a different product, or disabling
         // refresh
         if (!providerManagers.contains(currentProviderManager) || !enabled)
         {
            // If this is the last reference to the provider in the refresh map
            if (currentProviderManager->refresh_count() == 0)
            {
               // Disable current provider
               currentProviderManager->Disable();
            }
         }
      }

      // Dissociate uuid from current provider managers
      refreshMap_.erase(currentProviderManagers);
   }

   if (enabled)
   {
      // We are enabling provider managers
      // Associate uuid to provider manager
      refreshMap_.emplace(uuid, providerManagers);
      for (const auto& providerManager : providerManagers)
      {
         providerManager->increment_refresh_count();
      }
   }

   // Release the refresh map mutex
   lock.unlock();

   // We have already handled a disable request by this point. If enabling, and
   // the provider manager refresh isn't already enabled, enable it.
   if (enabled)
   {
      for (const auto& providerManager : providerManagers)
      {
         if (providerManager->refresh_enabled() != enabled)
         {
            providerManager->set_refresh_enabled(enabled);
            providerManager->RefreshData();
         }
      }
   }
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
   for (auto& refreshSet : p->refreshMap_)
   {
      for (const auto& refreshEntry : refreshSet.second)
      {
         // Add the provider for the current entry
         providers.insert(refreshEntry->provider());
      }
   }

   // Unlock the refresh map
   refreshLock.unlock();

   const auto today     = std::chrono::floor<std::chrono::days>(time);
   const auto yesterday = today - std::chrono::days {1};
   const auto tomorrow  = today + std::chrono::days {1};

   // For each provider (in parallel)
   std::for_each(
      std::execution::par,
      providers.begin(),
      providers.end(),
      [&](const std::shared_ptr<provider::NexradDataProvider>& provider)
      {
         const auto dates =
            provider->IsDateArchiveAvailable() ?
               std::initializer_list<std::chrono::system_clock::time_point> {
                  yesterday, today, tomorrow} :
               std::initializer_list<std::chrono::system_clock::time_point> {
                  today};

         // For yesterday, today and tomorrow (in parallel)
         std::for_each(
            std::execution::par,
            dates.begin(),
            dates.end(),
            [&](const auto& date)
            {
               // Don't query for a time point in the future
               if (date > scwx::util::time::now())
               {
                  return;
               }

               // Query the provider for volume time points
               auto timePoints = provider->GetTimePointsByDate(date, true);

               // TODO: Note, this will miss volume times present in Level 2
               // products with a second scan

               // Lock the merged volume time list
               const std::unique_lock volumeTimesLock {volumeTimesMutex};

               // Copy time points to the merged list
               std::copy(timePoints.begin(),
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
   logger_->trace("LoadProviderData: {}, {}",
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
                  logger_->trace(
                     "Data previously loaded, loading from data cache");
               }
            }
         }

         if (existingRecord == nullptr)
         {
            nexradFile = providerManager->provider()->LoadObjectByTime(time);
            if (nexradFile == nullptr)
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
   logger_->trace("LoadLevel2Data: {}", scwx::util::TimeString(time));

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
                     {
                        try
                        {
                           LoadNexradFile(load, request, mutex, time);
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void RadarProductManagerImpl::LoadNexradFile(
   CreateNexradFileFunction                           load,
   const std::shared_ptr<request::NexradFileRequest>& request,
   std::mutex&                                        mutex,
   std::chrono::system_clock::time_point              time)
{
   std::unique_lock lock {mutex};

   std::shared_ptr<wsr88d::NexradFile> nexradFile = load();

   std::shared_ptr<types::RadarProductRecord> record  = nullptr;
   std::shared_ptr<RadarProductManager>       manager = nullptr;

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

      manager = RadarProductManager::Instance(recordRadarId);
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

bool RadarProductManagerImpl::AreLevel2ProductTimesPopulated(
   std::chrono::system_clock::time_point time) const
{
   return AreProductTimesPopulated(level2ProviderManager_, time);
}

bool RadarProductManagerImpl::AreLevel3ProductTimesPopulated(
   const std::string& product, std::chrono::system_clock::time_point time)
{
   // Get provider manager
   const auto level3ProviderManager = GetLevel3ProviderManager(product);

   return AreProductTimesPopulated(level3ProviderManager, time);
}

bool RadarProductManagerImpl::AreProductTimesPopulated(
   const std::shared_ptr<ProviderManager>& providerManager,
   std::chrono::system_clock::time_point   time)
{
   auto today = std::chrono::floor<std::chrono::days>(time);

   bool productTimesPopulated = true;

   // Assume a query for the epoch is a query for now
   if (today == std::chrono::system_clock::time_point {})
   {
      today = std::chrono::floor<std::chrono::days>(scwx::util::time::now());
   }

   const auto yesterday = today - std::chrono::days {1};
   const auto tomorrow  = today + std::chrono::days {1};
   const auto dates =
      providerManager->provider()->IsDateArchiveAvailable() ?
         std::initializer_list<std::chrono::system_clock::time_point> {
            yesterday, today, tomorrow} :
         std::initializer_list<std::chrono::system_clock::time_point> {today};

   for (auto& date : dates)
   {
      // Don't query for a time point in the future
      if (date > scwx::util::time::now())
      {
         continue;
      }

      if (!providerManager->provider()->IsDateCached(date))
      {
         productTimesPopulated = false;
      }
   }

   return productTimesPopulated;
}

void RadarProductManagerImpl::PopulateLevel2ProductTimes(
   std::chrono::system_clock::time_point time, bool update)
{
   PopulateProductTimes(level2ProviderManager_,
                        level2ProductRecords_,
                        level2ProductRecordMutex_,
                        time,
                        update);
   PopulateProductTimes(level2ChunksProviderManager_,
                        level2ProductRecords_,
                        level2ProductRecordMutex_,
                        time,
                        update);
}

void RadarProductManagerImpl::PopulateLevel3ProductTimes(
   const std::string&                    product,
   std::chrono::system_clock::time_point time,
   bool                                  update)
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
                        time,
                        update);
}

void RadarProductManagerImpl::PopulateProductTimes(
   std::shared_ptr<ProviderManager>      providerManager,
   RadarProductRecordMap&                productRecordMap,
   std::shared_mutex&                    productRecordMutex,
   std::chrono::system_clock::time_point time,
   bool                                  update)
{
   if (update)
   {
      logger_->debug("Populating product times: {}, {}, {}",
                     common::GetRadarProductGroupName(providerManager->group()),
                     providerManager->product(),
                     scwx::util::time::TimeString(time));
   }
   else
   {
      logger_->trace("Populating cached product times: {}, {}, {}",
                     common::GetRadarProductGroupName(providerManager->group()),
                     providerManager->product(),
                     scwx::util::time::TimeString(time));
   }

   auto today = std::chrono::floor<std::chrono::days>(time);

   // Assume a query for the epoch is a query for now
   if (today == std::chrono::system_clock::time_point {})
   {
      today = std::chrono::floor<std::chrono::days>(scwx::util::time::now());
   }

   const auto yesterday = today - std::chrono::days {1};
   const auto tomorrow  = today + std::chrono::days {1};
   const auto dates =
      providerManager->provider()->IsDateArchiveAvailable() ?
         std::initializer_list<std::chrono::system_clock::time_point> {
            yesterday, today, tomorrow} :
         std::initializer_list<std::chrono::system_clock::time_point> {today};

   std::set<std::chrono::system_clock::time_point> volumeTimes {};
   std::mutex                                      volumeTimesMutex {};

   // For yesterday, today and tomorrow (in parallel)
   std::for_each(std::execution::par,
                 dates.begin(),
                 dates.end(),
                 [&](const auto& date)
                 {
                    // Don't query for a time point in the future
                    if (date > scwx::util::time::now())
                    {
                       return;
                    }

                    // Query the provider for volume time points
                    auto timePoints =
                       providerManager->provider()->GetTimePointsByDate(date,
                                                                        update);

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

std::tuple<std::map<std::chrono::system_clock::time_point,
                    std::shared_ptr<types::RadarProductRecord>>,
           types::RadarProductLoadStatus>
RadarProductManagerImpl::GetLevel2ProductRecords(
   std::chrono::system_clock::time_point time)
{
   std::map<std::chrono::system_clock::time_point,
            std::shared_ptr<types::RadarProductRecord>>
                                                     records {};
   std::vector<RadarProductRecordMap::const_pointer> recordPtrs {};
   types::RadarProductLoadStatus                     status {
      types::RadarProductLoadStatus::ListingProducts};

   std::size_t recordPtrCount = 0u;
   std::size_t recordCount    = 0u;

   // Ensure Level 2 product records are updated
   if (!AreLevel2ProductTimesPopulated(time))
   {
      logger_->debug("Level 2 product times need populated: {}",
                     scwx::util::time::TimeString(time));

      // Populate level 2 product times asynchronously
      boost::asio::post(threadPool_,
                        [time, this]()
                        {
                           // Populate product times
                           PopulateLevel2ProductTimes(time);

                           // Signal finished
                           Q_EMIT self_->ProductTimesPopulated(
                              common::RadarProductGroup::Level2, "", time);
                        });

      // Return listing products status
      return {records, status};
   }
   else
   {
      PopulateLevel2ProductTimes(time, false);
   }

   // Advance to loading product
   status = types::RadarProductLoadStatus::LoadingProduct;

   {
      std::shared_lock lock {level2ProductRecordMutex_};

      if (!level2ProductRecords_.empty() &&
          time == std::chrono::system_clock::time_point {})
      {
         // If a default-initialized time point is given, return the latest
         // record
         recordPtrs.push_back(&(*level2ProductRecords_.rbegin()));
      }
      else
      {
         // Get the requested record
         auto recordIt =
            scwx::util::GetBoundedElementIterator(level2ProductRecords_, time);

         if (recordIt != level2ProductRecords_.cend())
         {
            recordPtrs.push_back(&(*(recordIt)));

            // The requested time may be in the previous record, so get that too
            if (recordIt != level2ProductRecords_.cbegin())
            {
               recordPtrs.push_back(&(*(--recordIt)));
            }
         }
      }
   }

   // For each record pointer
   for (auto& recordPtr : recordPtrs)
   {
      std::shared_ptr<types::RadarProductRecord> record {nullptr};
      std::chrono::system_clock::time_point      recordTime {time};

      if (recordPtr != nullptr)
      {
         using namespace std::chrono_literals;

         // Don't check for an exact time match for level 2 products
         recordTime = recordPtr->first;

         if (
            // For latest data, ensure it is from the last 24 hours
            (time == std::chrono::system_clock::time_point {} &&
             (recordTime > scwx::util::time::now() - 24h ||
              recordTime == time)) ||
            // For time queries, ensure data is within 24 hours of the request
            (time != std::chrono::system_clock::time_point {} &&
             std::chrono::abs(recordTime - time) < 24h))
         {
            record = recordPtr->second.lock();
            ++recordPtrCount;
         }
         else
         {
            // Reset the record
            recordPtr  = nullptr;
            recordTime = time;
         }
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

         // Status is already set to LoadingProduct
      }

      if (record != nullptr)
      {
         // Return valid records
         records.insert_or_assign(recordTime, record);
         ++recordCount;
      }
   }

   if (recordPtrCount == 0)
   {
      // If all records are empty, the product is not available
      status = types::RadarProductLoadStatus::ProductNotAvailable;
   }
   else if (recordCount == recordPtrCount)
   {
      // If all records were populated, the product has been loaded
      status = types::RadarProductLoadStatus::ProductLoaded;
   }

   return {records, status};
}

std::tuple<std::shared_ptr<types::RadarProductRecord>,
           std::chrono::system_clock::time_point,
           types::RadarProductLoadStatus>
RadarProductManagerImpl::GetLevel3ProductRecord(
   const std::string& product, std::chrono::system_clock::time_point time)
{
   std::shared_ptr<types::RadarProductRecord> record {nullptr};
   RadarProductRecordMap::const_pointer       recordPtr {nullptr};
   std::chrono::system_clock::time_point      recordTime {time};
   types::RadarProductLoadStatus              status {
      types::RadarProductLoadStatus::ListingProducts};

   // Ensure Level 3 product records are updated
   if (!AreLevel3ProductTimesPopulated(product, time))
   {
      logger_->debug("Level 3 product times need populated: {}, {}",
                     product,
                     scwx::util::time::TimeString(time));

      // Populate level 3 product times asynchronously
      boost::asio::post(threadPool_,
                        [product, time, this]()
                        {
                           // Populate product times
                           PopulateLevel3ProductTimes(product, time);

                           // Signal finished
                           Q_EMIT self_->ProductTimesPopulated(
                              common::RadarProductGroup::Level3, product, time);
                        });

      // Return listing products status
      return {record, recordTime, status};
   }
   else
   {
      PopulateLevel3ProductTimes(product, time, false);
   }

   // Advance to loading product
   status = types::RadarProductLoadStatus::LoadingProduct;

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
      using namespace std::chrono_literals;

      // Don't check for an exact time match for level 3 products
      recordTime = recordPtr->first;

      if (
         // For latest data, ensure it is from the last 24 hours
         (time == std::chrono::system_clock::time_point {} &&
          (recordTime > scwx::util::time::now() - 24h || recordTime == time)) ||
         // For time queries, ensure data is within 24 hours of the request
         (time != std::chrono::system_clock::time_point {} &&
          std::chrono::abs(recordTime - time) < 24h))
      {
         record = recordPtr->second.lock();
      }
      else
      {
         // Reset the record
         recordPtr  = nullptr;
         recordTime = time;
      }
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

      // Status is already set to LoadingProduct
   }

   if (recordPtr == nullptr)
   {
      // If the record is empty, the product is not available
      status = types::RadarProductLoadStatus::ProductNotAvailable;
   }
   else if (record != nullptr)
   {
      // If the record was populated, the product has been loaded
      status = types::RadarProductLoadStatus::ProductLoaded;
   }

   return {record, recordTime, status};
}

std::shared_ptr<types::RadarProductRecord>
RadarProductManagerImpl::StoreRadarProductRecord(
   std::shared_ptr<types::RadarProductRecord> record)
{
   logger_->trace("StoreRadarProductRecord()");

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
           std::chrono::system_clock::time_point,
           types::RadarProductLoadStatus>
RadarProductManager::GetLevel2Data(wsr88d::rda::DataBlockType dataBlockType,
                                   float                      elevation,
                                   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<wsr88d::rda::ElevationScan> radarData    = nullptr;
   float                                       elevationCut = 0.0f;
   std::vector<float>                          elevationCuts {};
   std::chrono::system_clock::time_point       foundTime {};
   types::RadarProductLoadStatus               loadStatus {
      types::RadarProductLoadStatus::ProductNotLoaded};

   const bool        isEpox = time == std::chrono::system_clock::time_point {};
   bool              needArchive   = true;
   static const auto maxChunkDelay = std::chrono::minutes(10);
   const std::chrono::system_clock::time_point firstValidChunkTime =
      (isEpox ? scwx::util::time::now() : time) - maxChunkDelay;

   // See if we have this one in the chunk provider.
   auto chunkFile = std::dynamic_pointer_cast<wsr88d::Ar2vFile>(
      p->level2ChunksProviderManager_->provider()->LoadObjectByTime(time));
   if (chunkFile != nullptr)
   {
      std::tie(radarData, elevationCut, elevationCuts) =
         chunkFile->GetElevationScan(dataBlockType, elevation, time);

      if (radarData != nullptr)
      {
         auto& radarData0 = (*radarData)[0];
         foundTime        = std::chrono::floor<std::chrono::seconds>(
            scwx::util::TimePoint(radarData0->modified_julian_date(),
                                  radarData0->collection_time()));

         const std::optional<float> incomingElevation =
            std::dynamic_pointer_cast<provider::AwsLevel2ChunksDataProvider>(
               p->level2ChunksProviderManager_->provider())
               ->GetCurrentElevation();
         if (incomingElevation != p->incomingLevel2Elevation_)
         {
            p->incomingLevel2Elevation_ = incomingElevation;
            Q_EMIT IncomingLevel2ElevationChanged(incomingElevation);
         }

         if (foundTime >= firstValidChunkTime)
         {
            needArchive = false;
            loadStatus  = types::RadarProductLoadStatus::ProductLoaded;
         }
      }
   }

   // It is not in the chunk provider, so get it from the archive
   if (needArchive)
   {
      std::map<std::chrono::system_clock::time_point,
               std::shared_ptr<types::RadarProductRecord>>
         records;

      std::tie(records, loadStatus) = p->GetLevel2ProductRecords(time);
      for (auto& recordPair : records)
      {
         auto& record = recordPair.second;

         if (record != nullptr)
         {
            std::shared_ptr<wsr88d::rda::ElevationScan> recordRadarData =
               nullptr;
            float              recordElevationCut = 0.0f;
            std::vector<float> recordElevationCuts;

            std::tie(recordRadarData, recordElevationCut, recordElevationCuts) =
               record->level2_file()->GetElevationScan(
                  dataBlockType, elevation, time);

            if (recordRadarData != nullptr)
            {
               auto& radarData0     = (*recordRadarData)[0];
               auto  collectionTime = std::chrono::floor<std::chrono::seconds>(
                  scwx::util::TimePoint(radarData0->modified_julian_date(),
                                        radarData0->collection_time()));

               // Find the newest radar data, not newer than the selected time
               if (radarData == nullptr ||
                   (collectionTime <= time && foundTime < collectionTime) ||
                   (isEpox && foundTime < collectionTime))
               {
                  radarData     = recordRadarData;
                  elevationCut  = recordElevationCut;
                  elevationCuts = std::move(recordElevationCuts);
                  foundTime     = collectionTime;

                  if (!p->incomingLevel2Elevation_.has_value())
                  {
                     p->incomingLevel2Elevation_ = {};
                     Q_EMIT IncomingLevel2ElevationChanged(
                        p->incomingLevel2Elevation_);
                  }
               }
            }
         }
      }
   }

   if (loadStatus == types::RadarProductLoadStatus::ProductLoaded &&
       radarData == nullptr)
   {
      // If all data was available for the time point, but there is no matching
      // radar data, consider this as no product available
      loadStatus = types::RadarProductLoadStatus::ProductNotAvailable;
   }

   return {radarData, elevationCut, elevationCuts, foundTime, loadStatus};
}

std::tuple<std::shared_ptr<wsr88d::rpg::Level3Message>,
           std::chrono::system_clock::time_point,
           types::RadarProductLoadStatus>
RadarProductManager::GetLevel3Data(const std::string& product,
                                   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<wsr88d::rpg::Level3Message> message = nullptr;
   types::RadarProductLoadStatus               status {};

   std::shared_ptr<types::RadarProductRecord> record;
   std::tie(record, time, status) = p->GetLevel3ProductRecord(product, time);

   if (record != nullptr)
   {
      message = record->level3_file()->message();
   }

   return {message, time, status};
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
   return level3ProviderManager->provider()->GetAvailableProducts();
}

void RadarProductManager::SetCacheLimit(size_t cacheLimit)
{
   p->cacheLimit_ = std::max<std::size_t>(cacheLimit, 6u);
}

void RadarProductManager::UpdateAvailableProducts()
{
   std::lock_guard<std::mutex> guard(p->level3ProductsInitializeMutex_);

   if (p->level3ProductsInitialized_)
   {
      if (p->level3AvailabilityReady_)
      {
         // Multiple maps may use the same manager, so this ensures that all get
         // notified of the change
         Q_EMIT Level3ProductsChanged();
      }
      return;
   }

   // Although not complete here, only initialize once. Signal will be emitted
   // once complete.
   p->level3ProductsInitialized_ = true;

   logger_->debug("UpdateAvailableProducts()");

   boost::asio::post(p->threadPool_,
                     [this]()
                     {
                        try
                        {
                           p->UpdateAvailableProductsSync();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void RadarProductManagerImpl::UpdateAvailableProductsSync()
{
   auto level3ProviderManager =
      GetLevel3ProviderManager(kDefaultLevel3Product_);
   level3ProviderManager->provider()->RequestAvailableProducts();
   auto updatedAwipsIdList =
      level3ProviderManager->provider()->GetAvailableProducts();

   std::unique_lock lock {availableCategoryMutex_};

   for (common::Level3ProductCategory category :
        common::Level3ProductCategoryIterator())
   {
      const auto& products = common::GetLevel3ProductsByCategory(category);

      std::unordered_map<std::string, std::vector<std::string>>
         availableProducts;

      for (const auto& product : products)
      {
         const auto& awipsIds = common::GetLevel3AwipsIdsByProduct(product);

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
            availableProducts.insert_or_assign(product,
                                               std::move(availableAwipsIds));
         }
      }

      if (!availableProducts.empty())
      {
         availableCategoryMap_.insert_or_assign(category,
                                                std::move(availableProducts));
      }
      else
      {
         availableCategoryMap_.erase(category);
      }
   }

   level3AvailabilityReady_ = true;
   Q_EMIT self_->Level3ProductsChanged();
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

} // namespace scwx::qt::manager
