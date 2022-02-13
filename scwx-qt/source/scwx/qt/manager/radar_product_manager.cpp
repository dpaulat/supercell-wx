#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/qt/config/radar_site.hpp>
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

static void LoadNexradFile(CreateNexradFileFunction load,
                           FileLoadCompleteFunction onComplete,
                           QObject*                 context);

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

static std::shared_mutex fileLoadMutex_;

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

   std::string radarId_;
   bool        initialized_;

   std::shared_ptr<config::RadarSite> radarSite_;

   std::vector<float> coordinates0_5Degree_;
   std::vector<float> coordinates1Degree_;

   std::map<std::chrono::system_clock::time_point,
            std::shared_ptr<wsr88d::Ar2vFile>>
      level2VolumeScans_;

   std::mutex fileLoadMutex_;
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

void RadarProductManager::LoadData(std::istream&            is,
                                   FileLoadCompleteFunction onComplete,
                                   QObject*                 context)
{
   LoadNexradFile(
      [=, &is]() -> std::shared_ptr<wsr88d::NexradFile> {
         return wsr88d::NexradFileFactory::Create(is);
      },
      onComplete,
      context);
}

void RadarProductManager::LoadFile(const std::string&       filename,
                                   FileLoadCompleteFunction onComplete,
                                   QObject*                 context)
{
   LoadNexradFile(
      [=]() -> std::shared_ptr<wsr88d::NexradFile> {
         return wsr88d::NexradFileFactory::Create(filename);
      },
      onComplete,
      context);
}

static void LoadNexradFile(CreateNexradFileFunction load,
                           FileLoadCompleteFunction onComplete,
                           QObject*                 context)
{
   scwx::util::async(
      [=]()
      {
         std::unique_lock                    lock(fileLoadMutex_);
         std::shared_ptr<wsr88d::NexradFile> nexradFile = load();

         // TODO: Store and index
         //       - Should this impact arguments sent back in onComplete?

         lock.unlock();

         if (onComplete != nullptr)
         {
            if (context == nullptr)
            {
               onComplete(nexradFile);
            }
            else
            {
               QMetaObject::invokeMethod(context,
                                         [=]() { onComplete(nexradFile); });
            }
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
            std::lock_guard<std::mutex> guard(p->fileLoadMutex_);

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
