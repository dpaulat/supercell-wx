#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <deque>
#include <execution>
#include <mutex>
#include <shared_mutex>

#include <boost/log/trivial.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <QMapbox>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ =
   "[scwx::qt::manager::radar_product_manager] ";

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

// TODO: Find a way to garbage collect this
static std::unordered_map<std::string, std::shared_ptr<RadarProductManager>>
   instanceMap_;

std::unordered_map<std::string, std::shared_ptr<types::RadarProductRecord>>
   fileIndex_;

static std::shared_mutex fileLoadMutex_;
static std::shared_mutex fileIndexMutex_;

class RadarProductManagerImpl
{
public:
   explicit RadarProductManagerImpl(const std::string& radarId) :
       radarId_ {radarId},
       initialized_ {false},
       radarSite_ {config::RadarSite::Get(radarId)}
   {
      if (radarSite_ == nullptr)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Radar site not found: \"" << radarId_ << "\"";
         radarSite_ = std::make_shared<config::RadarSite>();
      }
   }
   ~RadarProductManagerImpl() = default;

   std::shared_ptr<types::RadarProductRecord>
   StoreRadarProductRecord(std::shared_ptr<types::RadarProductRecord> record);

   static void
   LoadNexradFile(CreateNexradFileFunction                    load,
                  std::shared_ptr<request::NexradFileRequest> request);

   std::string radarId_;
   bool        initialized_;

   std::shared_ptr<config::RadarSite> radarSite_;

   std::vector<float> coordinates0_5Degree_;
   std::vector<float> coordinates1Degree_;

   std::map<std::chrono::system_clock::time_point,
            std::shared_ptr<types::RadarProductRecord>>
      level2ProductRecords_;
   std::unordered_map<std::string,
                      std::map<std::chrono::system_clock::time_point,
                               std::shared_ptr<types::RadarProductRecord>>>
      level3ProductRecords_;

   std::map<std::chrono::system_clock::time_point,
            std::shared_ptr<wsr88d::Ar2vFile>>
      level2VolumeScans_;
};

RadarProductManager::RadarProductManager(const std::string& radarId) :
    p(std::make_unique<RadarProductManagerImpl>(radarId))
{
}
RadarProductManager::~RadarProductManager() = default;

const std::vector<float>&
RadarProductManager::coordinates(common::RadialSize radialSize) const
{
   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree: return p->coordinates0_5Degree_;
   case common::RadialSize::_1Degree: return p->coordinates1Degree_;
   }

   throw std::exception("Invalid radial size");
}

std::shared_ptr<config::RadarSite> RadarProductManager::radar_site() const
{
   return p->radarSite_;
}

std::shared_ptr<const wsr88d::Ar2vFile> RadarProductManager::level2_data() const
{
   std::shared_ptr<const wsr88d::Ar2vFile> level2Data = nullptr;

   if (p->level2VolumeScans_.size() > 0)
   {
      level2Data = p->level2VolumeScans_.crbegin()->second;
   }

   return level2Data;
}

void RadarProductManager::Initialize()
{
   if (p->initialized_)
   {
      return;
   }

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Initialize()";

   boost::timer::cpu_timer timer;

   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   const QMapbox::Coordinate radar(p->radarSite_->latitude(),
                                   p->radarSite_->longitude());

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
         const float  range  = (gate + 1) * 250.0f;   // 0.25km gate size
         const size_t offset = radialGate * 2;

         double latitude;
         double longitude;

         geodesic.Direct(
            radar.first, radar.second, angle, range, latitude, longitude);

         coordinates0_5Degree[offset]     = latitude;
         coordinates0_5Degree[offset + 1] = longitude;
      });
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Coordinates (0.5 degree) calculated in "
      << timer.format(6, "%ws");

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
         const float  range  = (gate + 1) * 250.0f;  // 0.25km gate size
         const size_t offset = radialGate * 2;

         double latitude;
         double longitude;

         geodesic.Direct(
            radar.first, radar.second, angle, range, latitude, longitude);

         coordinates1Degree[offset]     = latitude;
         coordinates1Degree[offset + 1] = longitude;
      });
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Coordinates (1 degree) calculated in "
      << timer.format(6, "%ws");

   p->initialized_ = true;
}

void RadarProductManager::LoadData(
   std::istream& is, std::shared_ptr<request::NexradFileRequest> request)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "LoadData()";

   RadarProductManagerImpl::LoadNexradFile(
      [=, &is]() -> std::shared_ptr<wsr88d::NexradFile>
      { return wsr88d::NexradFileFactory::Create(is); },
      request);
}

void RadarProductManager::LoadFile(
   const std::string&                          filename,
   std::shared_ptr<request::NexradFileRequest> request)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "LoadFile(" << filename << ")";

   std::shared_ptr<types::RadarProductRecord> existingRecord = nullptr;

   {
      std::shared_lock lock {fileIndexMutex_};
      auto             it = fileIndex_.find(filename);
      if (it != fileIndex_.cend())
      {
         BOOST_LOG_TRIVIAL(debug)
            << logPrefix_ << "File previously loaded, loading from file cache";

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
         request);
   }
   else if (request != nullptr)
   {
      request->set_radar_product_record(existingRecord);
      emit request->RequestComplete(request);
   }
}

void RadarProductManagerImpl::LoadNexradFile(
   CreateNexradFileFunction                    load,
   std::shared_ptr<request::NexradFileRequest> request)
{
   scwx::util::async(
      [=]()
      {
         std::unique_lock                    lock(fileLoadMutex_);
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

void RadarProductManager::LoadLevel2Data(const std::string& filename)
{
   std::shared_ptr<wsr88d::Ar2vFile> ar2vFile =
      std::make_shared<wsr88d::Ar2vFile>();

   if (!p->initialized_)
   {
      Initialize();
   }

   bool success = ar2vFile->LoadFile(filename);
   if (!success)
   {
      return;
   }

   p->level2VolumeScans_[ar2vFile->start_time()] = ar2vFile;

   emit Level2DataLoaded();
}

std::shared_ptr<types::RadarProductRecord>
RadarProductManagerImpl::StoreRadarProductRecord(
   std::shared_ptr<types::RadarProductRecord> record)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "StoreRadarProductRecord()";

   std::shared_ptr<types::RadarProductRecord> storedRecord = record;

   if (record->radar_product_group() == common::RadarProductGroup::Level2)
   {
      auto it = level2ProductRecords_.find(record->time());
      if (it != level2ProductRecords_.cend())
      {
         BOOST_LOG_TRIVIAL(debug)
            << logPrefix_
            << "Level 2 product previously loaded, loading from cache";

         storedRecord = it->second;
      }
      else
      {
         level2ProductRecords_[record->time()] = record;
      }
   }
   else if (record->radar_product_group() == common::RadarProductGroup::Level3)
   {
      auto productMap = level3ProductRecords_[record->radar_product()];

      auto it = productMap.find(record->time());
      if (it != productMap.cend())
      {
         BOOST_LOG_TRIVIAL(debug)
            << logPrefix_
            << "Level 3 product previously loaded, loading from cache";

         storedRecord = it->second;
      }
      else
      {
         productMap[record->time()] = record;
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

   if (p->level2VolumeScans_.size() > 0)
   {
      std::tie(radarData, elevationCut, elevationCuts) =
         p->level2VolumeScans_.crbegin()->second->GetElevationScan(
            dataBlockType, elevation, time);
   }
   else
   {
      scwx::util::async(
         [&]()
         {
            std::unique_lock lock(fileLoadMutex_);

            BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Start load";

            QString filename = qgetenv("AR2V_FILE");
            if (!filename.isEmpty() && p->level2VolumeScans_.size() == 0)
            {
               LoadLevel2Data(filename.toUtf8().constData());
            }

            BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "End load";
         });
   }

   return std::tie(radarData, elevationCut, elevationCuts);
}

std::shared_ptr<RadarProductManager>
RadarProductManager::Instance(const std::string& radarSite)
{
   static std::mutex           instanceMutex;
   std::lock_guard<std::mutex> guard(instanceMutex);

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
