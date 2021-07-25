#include <scwx/qt/manager/radar_manager.hpp>
#include <scwx/common/constants.hpp>

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

static const std::string logPrefix_ = "[scwx::qt::manager::radar_manager] ";

static constexpr uint32_t NUM_RADIAL_GATES_0_5_DEGREE =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_RADIAL_GATES_1_DEGREE =
   common::MAX_1_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t NUM_COORIDNATES_0_5_DEGREE =
   NUM_RADIAL_GATES_0_5_DEGREE * 2;
static constexpr uint32_t NUM_COORIDNATES_1_DEGREE =
   NUM_RADIAL_GATES_1_DEGREE * 2;

class RadarManagerImpl
{
public:
   explicit RadarManagerImpl() {}
   ~RadarManagerImpl() = default;

   std::vector<float> coordinates0_5Degree_;
   std::vector<float> coordinates1Degree_;
};

RadarManager::RadarManager() : p(std::make_unique<RadarManagerImpl>()) {}
RadarManager::~RadarManager() = default;

RadarManager::RadarManager(RadarManager&&) noexcept = default;
RadarManager& RadarManager::operator=(RadarManager&&) noexcept = default;

const std::vector<float>&
RadarManager::coordinates(common::RadialSize radialSize) const
{
   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree: return p->coordinates0_5Degree_;
   case common::RadialSize::_1Degree: return p->coordinates1Degree_;
   }

   throw std::exception("Invalid radial size");
}

void RadarManager::Initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Initialize()";

   boost::timer::cpu_timer timer;

   // Calculate coordinates
   timer.start();
   std::vector<float>& coordinates0_5Degree = p->coordinates0_5Degree_;

   coordinates0_5Degree.resize(NUM_COORIDNATES_0_5_DEGREE);

   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   const QMapbox::Coordinate radar(38.6986, -90.6828);
   auto radialGates = boost::irange<uint32_t>(0, NUM_RADIAL_GATES_0_5_DEGREE);

   std::for_each(
      std::execution::par_unseq,
      radialGates.begin(),
      radialGates.end(),
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
}

} // namespace manager
} // namespace qt
} // namespace scwx
