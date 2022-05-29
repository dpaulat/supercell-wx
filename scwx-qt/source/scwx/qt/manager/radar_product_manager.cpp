#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/provider/level2_data_provider_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <deque>
#include <execution>
#include <mutex>
#include <shared_mutex>

#include <boost/asio/steady_timer.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <fmt/chrono.h>
#include <GeographicLib/Geodesic.hpp>
#include <QMapbox>

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
                 std::shared_ptr<types::RadarProductRecord>>
   RadarProductRecordMap;

static constexpr uint32_t NUM_RADIAL_GATES_0_5_DEGREE =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_RADIAL_GATES_1_DEGREE =
   common::MAX_1_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_COORIDNATES_0_5_DEGREE =
   NUM_RADIAL_GATES_0_5_DEGREE * 2;
static constexpr uint32_t NUM_COORIDNATES_1_DEGREE =
   NUM_RADIAL_GATES_1_DEGREE * 2;

static constexpr std::chrono::seconds kRetryInterval_ {15};

// TODO: Find a way to garbage collect this
static std::unordered_map<std::string, std::shared_ptr<RadarProductManager>>
                  instanceMap_;
static std::mutex instanceMutex_;

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
       radarSite_ {config::RadarSite::Get(radarId)},
       coordinates0_5Degree_ {},
       coordinates1Degree_ {},
       level2ProductRecords_ {},
       level3ProductRecords_ {},
       level2ProductRecordMutex_ {},
       level3ProductRecordMutex_ {},
       level2DataRefreshEnabled_ {false},
       level2DataRefreshTimer_ {util::io_context()},
       level2DataRefreshTimerMutex_ {},
       level2DataProvider_ {
          provider::Level2DataProviderFactory::Create(radarId)},
       initializeMutex_ {},
       loadLevel2DataMutex_ {}
   {
      if (radarSite_ == nullptr)
      {
         logger_->warn("Radar site not found: \"{}\"", radarId_);
         radarSite_ = std::make_shared<config::RadarSite>();
      }
   }
   ~RadarProductManagerImpl()
   {
      {
         std::unique_lock lock(level2DataRefreshTimerMutex_);
         level2DataRefreshEnabled_ = false;
         level2DataRefreshTimer_.cancel();
      }
   }

   RadarProductManager* self_;

   void RefreshLevel2Data();

   std::shared_ptr<types::RadarProductRecord>
   GetLevel2ProductRecord(std::chrono::system_clock::time_point time);
   std::shared_ptr<types::RadarProductRecord>
   GetLevel3ProductRecord(const std::string&                    product,
                          std::chrono::system_clock::time_point time);
   std::shared_ptr<types::RadarProductRecord>
   StoreRadarProductRecord(std::shared_ptr<types::RadarProductRecord> record);

   static void
   LoadNexradFile(CreateNexradFileFunction                    load,
                  std::shared_ptr<request::NexradFileRequest> request,
                  std::mutex&                                 mutex);

   std::string radarId_;
   bool        initialized_;

   std::shared_ptr<config::RadarSite> radarSite_;

   std::vector<float> coordinates0_5Degree_;
   std::vector<float> coordinates1Degree_;

   RadarProductRecordMap                                  level2ProductRecords_;
   std::unordered_map<std::string, RadarProductRecordMap> level3ProductRecords_;

   std::shared_mutex level2ProductRecordMutex_;
   std::shared_mutex level3ProductRecordMutex_;

   bool                                          level2DataRefreshEnabled_;
   boost::asio::steady_timer                     level2DataRefreshTimer_;
   std::mutex                                    level2DataRefreshTimerMutex_;
   std::shared_ptr<provider::Level2DataProvider> level2DataProvider_;

   std::mutex initializeMutex_;
   std::mutex loadLevel2DataMutex_;
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

const std::vector<float>&
RadarProductManager::coordinates(common::RadialSize radialSize) const
{
   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree:
      return p->coordinates0_5Degree_;
   case common::RadialSize::_1Degree:
      return p->coordinates1Degree_;
   }

   throw std::exception("Invalid radial size");
}

float RadarProductManager::gate_size() const
{
   return (p->radarSite_->type() == "tdwr") ? 150.0f : 250.0f;
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

   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   const QMapbox::Coordinate radar(p->radarSite_->latitude(),
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

         const float  angle  = radial * 0.5f - 0.25f; // 0.5 degree radial
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

         const float  angle  = radial * 1.0f - 0.5f; // 1 degree radial
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

void RadarProductManager::EnableLevel2Refresh(bool enabled)
{
   if (p->level2DataRefreshEnabled_ != enabled)
   {
      p->level2DataRefreshEnabled_ = enabled;

      if (enabled)
      {
         p->RefreshLevel2Data();
      }
   }
}

void RadarProductManagerImpl::RefreshLevel2Data()
{
   logger_->debug("RefreshLevel2Data()");

   {
      std::unique_lock lock(level2DataRefreshTimerMutex_);
      level2DataRefreshTimer_.cancel();
   }

   util::async(
      [&]()
      {
         size_t newObjects = level2DataProvider_->Refresh();

         std::chrono::milliseconds interval = kRetryInterval_;

         if (newObjects > 0)
         {
            std::string key = level2DataProvider_->FindLatestKey();
            auto latestTime = level2DataProvider_->GetTimePointByKey(key);

            auto updatePeriod = level2DataProvider_->update_period();
            auto lastModified = level2DataProvider_->last_modified();
            interval = std::chrono::duration_cast<std::chrono::milliseconds>(
               updatePeriod -
               (std::chrono::system_clock::now() - lastModified));
            if (interval < std::chrono::milliseconds {kRetryInterval_})
            {
               interval = kRetryInterval_;
            }

            emit self_->NewLevel2DataAvailable(latestTime);
         }

         std::unique_lock lock(level2DataRefreshTimerMutex_);

         if (level2DataRefreshEnabled_)
         {
            logger_->debug(
               "Scheduled refresh in {:%M:%S}",
               std::chrono::duration_cast<std::chrono::seconds>(interval));

            {
               level2DataRefreshTimer_.expires_after(interval);
               level2DataRefreshTimer_.async_wait(
                  [this](const boost::system::error_code& e)
                  {
                     if (e == boost::system::errc::success)
                     {
                        RefreshLevel2Data();
                     }
                     else if (e == boost::asio::error::operation_aborted)
                     {
                        logger_->debug("Level 2 data refresh timer cancelled");
                     }
                     else
                     {
                        logger_->warn("Level 2 data refresh timer error: {}",
                                      e.message());
                     }
                  });
            }
         }
      });
}

void RadarProductManager::LoadLevel2Data(
   std::chrono::system_clock::time_point       time,
   std::shared_ptr<request::NexradFileRequest> request)
{
   logger_->debug("LoadLevel2Data: {}", util::TimeString(time));

   RadarProductManagerImpl::LoadNexradFile(
      [=]() -> std::shared_ptr<wsr88d::NexradFile>
      {
         std::shared_ptr<types::RadarProductRecord> existingRecord = nullptr;
         std::shared_ptr<wsr88d::NexradFile>        nexradFile     = nullptr;

         {
            std::shared_lock sharedLock {p->level2ProductRecordMutex_};

            auto it = p->level2ProductRecords_.find(time);
            if (it != p->level2ProductRecords_.cend())
            {
               logger_->debug(
                  "Data previously loaded, loading from data cache");

               existingRecord = it->second;
            }
         }

         if (existingRecord == nullptr)
         {
            std::string key = p->level2DataProvider_->FindKey(time);
            nexradFile      = p->level2DataProvider_->LoadObjectByKey(key);
         }
         else
         {
            nexradFile = existingRecord->nexrad_file();
         }

         return nexradFile;
      },
      request,
      p->loadLevel2DataMutex_);
}

void RadarProductManager::LoadData(
   std::istream& is, std::shared_ptr<request::NexradFileRequest> request)
{
   logger_->debug("LoadData()");

   RadarProductManagerImpl::LoadNexradFile(
      [=, &is]() -> std::shared_ptr<wsr88d::NexradFile>
      { return wsr88d::NexradFileFactory::Create(is); },
      request,
      fileLoadMutex_);
}

void RadarProductManager::LoadFile(
   const std::string&                          filename,
   std::shared_ptr<request::NexradFileRequest> request)
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

      RadarProductManagerImpl::LoadNexradFile(
         [=]() -> std::shared_ptr<wsr88d::NexradFile>
         { return wsr88d::NexradFileFactory::Create(filename); },
         request,
         fileLoadMutex_);
   }
   else if (request != nullptr)
   {
      request->set_radar_product_record(existingRecord);
      emit request->RequestComplete(request);
   }
}

void RadarProductManagerImpl::LoadNexradFile(
   CreateNexradFileFunction                    load,
   std::shared_ptr<request::NexradFileRequest> request,
   std::mutex&                                 mutex)
{
   scwx::util::async(
      [=, &mutex]()
      {
         std::unique_lock lock {mutex};

         std::shared_ptr<wsr88d::NexradFile> nexradFile = load();

         std::shared_ptr<types::RadarProductRecord> record = nullptr;

         bool fileValid = (nexradFile != nullptr);

         if (fileValid)
         {
            record = types::RadarProductRecord::Create(nexradFile);

            std::shared_ptr<RadarProductManager> manager =
               RadarProductManager::Instance(record->radar_id());

            manager->Initialize();
            record = manager->p->StoreRadarProductRecord(record);
         }

         lock.unlock();

         if (request != nullptr)
         {
            request->set_radar_product_record(record);
            emit request->RequestComplete(request);
         }
      });
}

std::shared_ptr<types::RadarProductRecord>
RadarProductManagerImpl::GetLevel2ProductRecord(
   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<types::RadarProductRecord> record;

   if (!level2ProductRecords_.empty() &&
       time == std::chrono::system_clock::time_point {})
   {
      // If a default-initialized time point is given, return the latest record
      record = level2ProductRecords_.rbegin()->second;
   }
   else
   {
      // TODO: Round to minutes
      record = util::GetBoundedElementValue(level2ProductRecords_, time);

      // Does the record contain the time we are looking for?
      if (record != nullptr && (time < record->level2_file()->start_time() ||
                                record->level2_file()->end_time() < time))
      {
         record = nullptr;
      }
   }

   return record;
}

std::shared_ptr<types::RadarProductRecord>
RadarProductManagerImpl::GetLevel3ProductRecord(
   const std::string& product, std::chrono::system_clock::time_point time)
{
   std::shared_ptr<types::RadarProductRecord> record = nullptr;

   auto it = level3ProductRecords_.find(product);

   if (it != level3ProductRecords_.cend())
   {
      record = util::GetBoundedElementValue(it->second, time);
   }

   return record;
}

std::shared_ptr<types::RadarProductRecord>
RadarProductManagerImpl::StoreRadarProductRecord(
   std::shared_ptr<types::RadarProductRecord> record)
{
   logger_->debug("StoreRadarProductRecord()");

   std::shared_ptr<types::RadarProductRecord> storedRecord = record;

   auto timeInSeconds =
      std::chrono::time_point_cast<std::chrono::seconds,
                                   std::chrono::system_clock>(record->time());

   if (record->radar_product_group() == common::RadarProductGroup::Level2)
   {
      std::unique_lock lock {level2ProductRecordMutex_};

      auto it = level2ProductRecords_.find(timeInSeconds);
      if (it != level2ProductRecords_.cend())
      {
         logger_->debug(
            "Level 2 product previously loaded, loading from cache");

         storedRecord = it->second;
      }
      else
      {
         level2ProductRecords_[timeInSeconds] = record;
      }
   }
   else if (record->radar_product_group() == common::RadarProductGroup::Level3)
   {
      std::unique_lock lock {level3ProductRecordMutex_};

      auto& productMap = level3ProductRecords_[record->radar_product()];

      auto it = productMap.find(timeInSeconds);
      if (it != productMap.cend())
      {
         logger_->debug(
            "Level 3 product previously loaded, loading from cache");

         storedRecord = it->second;
      }
      else
      {
         productMap[timeInSeconds] = record;
      }
   }

   return storedRecord;
}

std::tuple<std::shared_ptr<wsr88d::rda::ElevationScan>,
           float,
           std::vector<float>>
RadarProductManager::GetLevel2Data(wsr88d::rda::DataBlockType dataBlockType,
                                   float                      elevation,
                                   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<wsr88d::rda::ElevationScan> radarData    = nullptr;
   float                                       elevationCut = 0.0f;
   std::vector<float>                          elevationCuts;

   std::shared_ptr<types::RadarProductRecord> record =
      p->GetLevel2ProductRecord(time);

   if (record != nullptr)
   {
      std::tie(radarData, elevationCut, elevationCuts) =
         record->level2_file()->GetElevationScan(
            dataBlockType, elevation, time);
   }

   return std::tie(radarData, elevationCut, elevationCuts);
}

std::shared_ptr<wsr88d::rpg::Level3Message>
RadarProductManager::GetLevel3Data(const std::string& product,
                                   std::chrono::system_clock::time_point time)
{
   std::shared_ptr<wsr88d::rpg::Level3Message> message = nullptr;

   std::shared_ptr<types::RadarProductRecord> record =
      p->GetLevel3ProductRecord(product, time);

   if (record != nullptr)
   {
      message = record->level3_file()->message();
   }

   return message;
}

std::shared_ptr<RadarProductManager>
RadarProductManager::Instance(const std::string& radarSite)
{
   std::lock_guard<std::mutex> guard(instanceMutex_);

   if (!instanceMap_.contains(radarSite))
   {
      instanceMap_[radarSite] =
         std::make_shared<RadarProductManager>(radarSite);
   }

   return instanceMap_[radarSite];
}

} // namespace manager
} // namespace qt
} // namespace scwx
