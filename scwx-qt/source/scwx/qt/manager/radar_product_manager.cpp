#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/threads.hpp>

#include <deque>
#include <execution>

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

static constexpr uint32_t NUM_RADIAL_GATES_0_5_DEGREE =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_RADIAL_GATES_1_DEGREE =
   common::MAX_1_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_COORIDNATES_0_5_DEGREE =
   NUM_RADIAL_GATES_0_5_DEGREE * 2;
static constexpr uint32_t NUM_COORIDNATES_1_DEGREE =
   NUM_RADIAL_GATES_1_DEGREE * 2;

// TODO: Configure this in settings for radar loop
static constexpr size_t MAX_LEVEL2_FILES = 6;

class RadarProductManagerImpl
{
public:
   explicit RadarProductManagerImpl() {}
   ~RadarProductManagerImpl() = default;

   std::vector<float> coordinates0_5Degree_;
   std::vector<float> coordinates1Degree_;

   std::deque<std::shared_ptr<wsr88d::Ar2vFile>> level2Data_;
};

RadarProductManager::RadarProductManager() :
    p(std::make_unique<RadarProductManagerImpl>())
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

std::shared_ptr<const wsr88d::Ar2vFile> RadarProductManager::level2_data() const
{
   std::shared_ptr<const wsr88d::Ar2vFile> level2Data = nullptr;

   if (p->level2Data_.size() > 0)
   {
      level2Data = p->level2Data_.back();
   }

   return level2Data;
}

void RadarProductManager::Initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Initialize()";

   boost::timer::cpu_timer timer;

   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   // TODO: This should be retrieved from configuration
   const QMapbox::Coordinate radar(38.6986, -90.6828);

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
      [&](uint32_t radialGate) {
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
      [&](uint32_t radialGate) {
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
}

void RadarProductManager::LoadLevel2Data(const std::string& filename)
{
   std::shared_ptr<wsr88d::Ar2vFile> ar2vFile =
      std::make_shared<wsr88d::Ar2vFile>();

   bool success = ar2vFile->LoadFile(filename);
   if (!success)
   {
      return;
   }

   // TODO: Sort and index these
   if (p->level2Data_.size() >= MAX_LEVEL2_FILES - 1)
   {
      p->level2Data_.pop_front();
   }
   p->level2Data_.push_back(ar2vFile);

   emit Level2DataLoaded();
}

std::unordered_map<uint16_t, std::shared_ptr<wsr88d::rda::DigitalRadarData>>
RadarProductManager::GetLevel2Data(wsr88d::rda::DataBlockType dataBlockType,
                                   uint8_t                    elevationIndex,
                                   std::chrono::system_clock::time_point time)
{
   std::unordered_map<uint16_t, std::shared_ptr<wsr88d::rda::DigitalRadarData>>
      radarData;

   if (p->level2Data_.size() > 0)
   {
      // TODO: Pull this from the database
      radarData = p->level2Data_[0]->radar_data()[elevationIndex];
   }
   else
   {
      scwx::util::async([&]() {
         QString filename = qgetenv("AR2V_FILE");
         if (!filename.isEmpty())
         {
            LoadLevel2Data(filename.toUtf8().constData());
         }
      });
   }

   return radarData;
}

} // namespace manager
} // namespace qt
} // namespace scwx
