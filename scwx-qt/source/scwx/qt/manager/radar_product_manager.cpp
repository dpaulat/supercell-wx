#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/provider_manager.hpp>
#include <scwx/qt/manager/product_datastore.hpp>
#include <scwx/qt/manager/radar_product_manager_notifier.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/time_types.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/provider/aws_level2_chunks_data_provider.hpp>
#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <execution>
#include <mutex>
#include <optional>
#include <shared_mutex>
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
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <fmt/chrono.h>
#include <GeographicLib/GeodesicLine.hpp>
#include <qmaplibre.hpp>
#include <units/angle.h>

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

static constexpr uint32_t NUM_RADIAL_GATES_0_5_DEGREE =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_RADIAL_GATES_1_DEGREE =
   common::MAX_1_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_COORIDNATES_0_5_DEGREE =
   NUM_RADIAL_GATES_0_5_DEGREE * 2;
static constexpr uint32_t NUM_COORIDNATES_1_DEGREE =
   NUM_RADIAL_GATES_1_DEGREE * 2;
static constexpr uint32_t NUM_RADIALS_0_5_DEGREE =
   common::MAX_0_5_DEGREE_RADIALS;
static constexpr uint32_t NUM_RADIALS_1_DEGREE = common::MAX_1_DEGREE_RADIALS;

// Coordinate grid tuning: radial step (deg), extra azimuth when smoothing is
// on, and first-gate fractional offset into slant range (matches prior layout).
static constexpr float kCoordinateRadialStepDegrees0_5_ {0.5F};
static constexpr float kCoordinateSmoothingRadialOffsetDegrees0_5_ {0.25F};
static constexpr float kCoordinateSmoothingRadialOffsetDegrees1_0_ {0.5F};
static constexpr float kCoordinateSmoothingGateRangeOffset_ {0.5F};
static constexpr float kCoordinateNoSmoothingGateRangeOffset_ {1.0F};

static const std::string kDefaultLevel3Product_ {"N0B"};

static constexpr std::size_t kTimerPlaces_ {6u};

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

   void LoadNexradFileAsync(
      CreateNexradFileFunction                           load,
      const std::shared_ptr<request::NexradFileRequest>& request,
      std::mutex&                                        mutex,
      std::chrono::system_clock::time_point              time);
   void
   LoadProviderData(std::chrono::system_clock::time_point time,
                    std::shared_ptr<ProviderManager>      providerManager,
                    const std::string&                    level3Product,
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

   void CalculateCoordinates(uint32_t                           radialCount,
                             const units::angle::degrees<float> radialAngle,
                             const units::angle::degrees<float> angleOffset,
                             float                              gateRangeOffset,
                             std::vector<float>& outputCoordinates);
   void EnsureCoordinatesInitialized(common::RadialSize radialSize,
                                     bool               smoothingEnabled);

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

   ProductDatastore productDatastore_ {};

   // Lat/lon per radial/gate. Filled on first coordinates() for each table, not
   // in Initialize(). coordinatesMutex_ serializes checks and builds so callers
   // never observe a partially filled table.
   std::vector<float> coordinates0_5Degree_ {};
   std::vector<float> coordinates0_5DegreeSmooth_ {};
   std::vector<float> coordinates1Degree_ {};
   std::vector<float> coordinates1DegreeSmooth_ {};

   std::shared_ptr<ProviderManager> level2ProviderManager_;
   std::shared_ptr<ProviderManager> level2ChunksProviderManager_;
   std::unordered_map<std::string, std::shared_ptr<ProviderManager>>
                     level3ProviderManagerMap_ {};
   std::shared_mutex level3ProviderManagerMutex_ {};

   std::mutex initializeMutex_ {};
   std::mutex coordinatesMutex_ {};
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
                  radarProductManager->p->productDatastore_.ForEachLevel2Record(
                     [&](std::chrono::system_clock::time_point recordTime,
                         bool                                  expired)
                     {
                        logger_->info("   {}{}",
                                      scwx::util::TimeString(recordTime),
                                      expired ? " (expired)" : "");
                     });
               }

               logger_->info("  Level 3");

               {
                  radarProductManager->p->productDatastore_
                     .ForEachLevel3Product(
                        [&](const std::string&           product,
                            const RadarProductRecordMap& recordMap)
                        {
                           logger_->info("   {}", product);

                           for (auto& record : recordMap)
                           {
                              logger_->info(
                                 "    {}{}",
                                 scwx::util::TimeString(record.first),
                                 record.second.expired() ? " (expired)" : "");
                           }
                        });
               }
            }
         }
      });
}

// Cached lat/lon grid; first use for a (radialSize, smoothing) pair may block
// on EnsureCoordinatesInitialized.
const std::vector<float>&
RadarProductManager::coordinates(common::RadialSize radialSize,
                                 bool               smoothingEnabled) const
{
   p->EnsureCoordinatesInitialized(radialSize, smoothingEnabled);

   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree:
      if (smoothingEnabled)
      {
         return p->coordinates0_5DegreeSmooth_;
      }
      else
      {
         return p->coordinates0_5Degree_;
      }
   case common::RadialSize::_1Degree:
      if (smoothingEnabled)
      {
         return p->coordinates1DegreeSmooth_;
      }
      else
      {
         return p->coordinates1Degree_;
      }
   default:
      throw std::invalid_argument("Invalid radial size");
   }
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

bool RadarProductManager::is_tdwr() const
{
   return p->radarSite_->type() == "tdwr";
}

float RadarProductManager::gate_size() const
{
   // tdwr is 150 meter per gate, wsr88d is 250 meter per gate
   // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
   return (is_tdwr()) ? 150.0f : 250.0f;
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

   if (is_tdwr())
   {
      p->initialized_ = true;
      return;
   }

   p->initialized_ = true;
}

// One GeodesicLine per radial (Position per gate). Parallelism is over radials.
void RadarProductManagerImpl::CalculateCoordinates(
   uint32_t                           radialCount,
   const units::angle::degrees<float> radialAngle,
   const units::angle::degrees<float> angleOffset,
   float                              gateRangeOffset,
   std::vector<float>&                outputCoordinates)
{
   const GeographicLib::Geodesic& geodesic(
      util::GeographicLib::DefaultGeodesic());

   const QMapLibre::Coordinate radar(radarSite_->latitude(),
                                     radarSite_->longitude());

   const float gateSize = self_->gate_size();

   const auto   radials = boost::irange<uint32_t>(0, radialCount);
   const double radarLatitude {radar.first};
   const double radarLongitude {radar.second};
   const float  radialStep {radialAngle.value()};
   const float  radialOffset {angleOffset.value()};

   std::for_each(
      std::execution::par_unseq,
      radials.begin(),
      radials.end(),
      [&](uint32_t radial)
      {
         const float angle =
            static_cast<float>(radial) * radialStep + radialOffset;
         const auto geodesicLine =
            geodesic.Line(radarLatitude, radarLongitude, angle);
         const std::size_t baseOffset =
            static_cast<std::size_t>(radial) *
            static_cast<std::size_t>(common::MAX_DATA_MOMENT_GATES) * 2;

         for (uint32_t gate = 0; gate < common::MAX_DATA_MOMENT_GATES; ++gate)
         {
            const float range =
               (static_cast<float>(gate) + gateRangeOffset) * gateSize;
            const std::size_t offset =
               baseOffset + static_cast<std::size_t>(gate) * 2;

            double latitude  = 0.0;
            double longitude = 0.0;

            geodesicLine.Position(range, latitude, longitude);

            outputCoordinates[offset]     = static_cast<float>(latitude);
            outputCoordinates[offset + 1] = static_cast<float>(longitude);
         }
      });
}

// One-time table fill under coordinatesMutex_. All readiness checks and writes
// stay under the same lock so no thread sees non-empty after resize() before
// CalculateCoordinates() finishes (avoids data races on empty()/size()).
void RadarProductManagerImpl::EnsureCoordinatesInitialized(
   common::RadialSize radialSize, bool smoothingEnabled)
{
   std::vector<float>*                         targetCoordinates = nullptr;
   uint32_t                                    coordinateCount   = 0;
   uint32_t                                    radialCount       = 0;
   std::optional<units::angle::degrees<float>> radialAngle {};
   std::optional<units::angle::degrees<float>> angleOffset {};
   float                                       gateRangeOffset = 0.0f;
   const char*                                 timerName       = nullptr;

   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree:
      targetCoordinates = smoothingEnabled ? &coordinates0_5DegreeSmooth_ :
                                             &coordinates0_5Degree_;
      coordinateCount   = NUM_COORIDNATES_0_5_DEGREE;
      radialCount       = NUM_RADIALS_0_5_DEGREE;
      radialAngle =
         units::angle::degrees<float> {kCoordinateRadialStepDegrees0_5_};
      angleOffset = units::angle::degrees<float> {
         smoothingEnabled ? kCoordinateSmoothingRadialOffsetDegrees0_5_ : 0.0F};
      gateRangeOffset = smoothingEnabled ?
                           kCoordinateSmoothingGateRangeOffset_ :
                           kCoordinateNoSmoothingGateRangeOffset_;
      timerName       = smoothingEnabled ? "Coordinates (0.5 degree smooth)" :
                                           "Coordinates (0.5 degree)";
      break;
   case common::RadialSize::_1Degree:
      targetCoordinates =
         smoothingEnabled ? &coordinates1DegreeSmooth_ : &coordinates1Degree_;
      coordinateCount = NUM_COORIDNATES_1_DEGREE;
      radialCount     = NUM_RADIALS_1_DEGREE;
      radialAngle     = units::angle::degrees<float> {1.0f};
      angleOffset     = units::angle::degrees<float> {
         smoothingEnabled ? kCoordinateSmoothingRadialOffsetDegrees1_0_ : 0.0F};
      gateRangeOffset = smoothingEnabled ?
                           kCoordinateSmoothingGateRangeOffset_ :
                           kCoordinateNoSmoothingGateRangeOffset_;
      timerName       = smoothingEnabled ? "Coordinates (1 degree smooth)" :
                                           "Coordinates (1 degree)";
      break;
   default:
      return;
   }

   std::unique_lock<std::mutex> const lock {coordinatesMutex_};
   if (targetCoordinates == nullptr || !targetCoordinates->empty())
   {
      return;
   }

   // Switch arms above always set these; keeps static analysis safe if enum
   // grows.
   if (!radialAngle.has_value() || !angleOffset.has_value())
   {
      return;
   }

   targetCoordinates->resize(coordinateCount);

   boost::timer::cpu_timer timer {};
   timer.start();
   CalculateCoordinates(radialCount,
                        radialAngle.value(),
                        angleOffset.value(),
                        gateRangeOffset,
                        *targetCoordinates);
   timer.stop();
   logger_->debug(
      "{} calculated in {}", timerName, timer.format(kTimerPlaces_, "%ws"));
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
   const auto dates     = {yesterday, today, tomorrow};

   // For each provider (in parallel)
   std::for_each(
      std::execution::par,
      providers.begin(),
      providers.end(),
      [&](const std::shared_ptr<provider::NexradDataProvider>& provider)
      {
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
   const std::string&                                 level3Product,
   std::mutex&                                        loadDataMutex,
   const std::shared_ptr<request::NexradFileRequest>& request)
{
   logger_->trace("LoadProviderData: {}, {}",
                  providerManager->name(),
                  scwx::util::TimeString(time));

   LoadNexradFileAsync(
      [providerManager, level3Product, time, this]()
         -> std::shared_ptr<wsr88d::NexradFile>
      {
         std::shared_ptr<wsr88d::NexradFile> nexradFile =
            productDatastore_.GetCachedNexradFile(
               providerManager->group(), level3Product, time);

         if (nexradFile == nullptr)
         {
            nexradFile = providerManager->provider()->LoadObjectByTime(time);
            if (nexradFile == nullptr)
            {
               logger_->warn("Attempting to load object without key: {}",
                             scwx::util::TimeString(time));
            }
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

   p->LoadProviderData(
      time, p->level2ProviderManager_, "", p->loadLevel2DataMutex_, request);
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

   // Load provider data
   p->LoadProviderData(time,
                       level3ProviderManager->second,
                       product,
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
      record = manager->p->productDatastore_.Store(record);
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
   return ProductDatastore::AreProductTimesPopulated(level2ProviderManager_,
                                                     time) &&
          ProductDatastore::AreProductTimesPopulated(
             level2ChunksProviderManager_, time);
}

bool RadarProductManagerImpl::AreLevel3ProductTimesPopulated(
   const std::string& product, std::chrono::system_clock::time_point time)
{
   const auto level3ProviderManager = GetLevel3ProviderManager(product);

   return ProductDatastore::AreProductTimesPopulated(level3ProviderManager,
                                                     time);
}

void RadarProductManagerImpl::PopulateLevel2ProductTimes(
   std::chrono::system_clock::time_point time, bool update)
{
   productDatastore_.PopulateLevel2ProductTimes(
      level2ProviderManager_, level2ChunksProviderManager_, time, update);
}

void RadarProductManagerImpl::PopulateLevel3ProductTimes(
   const std::string&                    product,
   std::chrono::system_clock::time_point time,
   bool                                  update)
{
   auto level3ProviderManager = GetLevel3ProviderManager(product);

   productDatastore_.PopulateLevel3ProductTimes(
      level3ProviderManager, product, time, update);
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
   types::RadarProductLoadStatus status {
      types::RadarProductLoadStatus::ListingProducts};

   std::size_t recordCount = 0u;

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

   const auto recordEntries = productDatastore_.FindLevel2RecordEntries(time);

   std::size_t validEntryCount = 0u;

   // For each record entry
   for (const auto& recordEntry : recordEntries)
   {
      std::shared_ptr<types::RadarProductRecord> record {nullptr};
      std::chrono::system_clock::time_point      recordTime {time};
      bool                                       entryValid = true;

      using namespace std::chrono_literals;

      // Don't check for an exact time match for level 2 products
      recordTime = recordEntry.time;

      if (
         // For latest data, ensure it is from the last 24 hours
         (time == std::chrono::system_clock::time_point {} &&
          (recordTime > scwx::util::time::now() - 24h || recordTime == time)) ||
         // For time queries, ensure data is within 24 hours of the request
         (time != std::chrono::system_clock::time_point {} &&
          std::chrono::abs(recordTime - time) < 24h))
      {
         record = recordEntry.record.lock();
         ++validEntryCount;
      }
      else
      {
         entryValid = false;
         recordTime = time;
      }

      if (entryValid && record == nullptr &&
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

   if (validEntryCount == 0)
   {
      // If all records are empty, the product is not available
      status = types::RadarProductLoadStatus::ProductNotAvailable;
   }
   else if (recordCount == validEntryCount)
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
   std::chrono::system_clock::time_point      recordTime {time};
   types::RadarProductLoadStatus              status {
      types::RadarProductLoadStatus::ListingProducts};
   bool recordEntryValid = false;

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

   const auto recordEntry =
      productDatastore_.FindLevel3RecordEntry(product, time);

   if (recordEntry.has_value())
   {
      using namespace std::chrono_literals;

      recordEntryValid = true;

      // Don't check for an exact time match for level 3 products
      recordTime = recordEntry->time;

      if (
         // For latest data, ensure it is from the last 24 hours
         (time == std::chrono::system_clock::time_point {} &&
          (recordTime > scwx::util::time::now() - 24h || recordTime == time)) ||
         // For time queries, ensure data is within 24 hours of the request
         (time != std::chrono::system_clock::time_point {} &&
          std::chrono::abs(recordTime - time) < 24h))
      {
         record = recordEntry->record.lock();
      }
      else
      {
         recordEntryValid = false;
         recordTime       = time;
      }
   }

   if (recordEntryValid && record == nullptr &&
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

   if (!recordEntryValid)
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
   p->productDatastore_.SetCacheLimit(cacheLimit);
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
