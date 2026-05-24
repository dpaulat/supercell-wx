#include <scwx/qt/manager/radar_coordinate_table.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/util/logger.hpp>

#include <execution>
#include <optional>

#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/GeodesicLine.hpp>

namespace scwx::qt::manager
{

namespace
{

static const std::string logPrefix_ =
   "scwx::qt::manager::radar_coordinate_table";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr uint32_t kNumRadialGates0_5Degree_ =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t kNumRadialGates1Degree_ =
   common::MAX_1_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr uint32_t kNumCoordinates0_5Degree_ =
   kNumRadialGates0_5Degree_ * 2;
static constexpr uint32_t kNumCoordinates1Degree_ = kNumRadialGates1Degree_ * 2;
static constexpr uint32_t kNumRadials0_5Degree_ =
   common::MAX_0_5_DEGREE_RADIALS;
static constexpr uint32_t kNumRadials1Degree_ = common::MAX_1_DEGREE_RADIALS;

static constexpr float kRadialStepDegrees0_5_ {0.5F};
static constexpr float kSmoothingRadialOffsetDegrees0_5_ {0.25F};
static constexpr float kSmoothingRadialOffsetDegrees1_0_ {0.5F};
static constexpr float kSmoothingGateRangeOffset_ {0.5F};
static constexpr float kNoSmoothingGateRangeOffset_ {1.0F};

static constexpr std::size_t kTimerPlaces_ {6u};

} // namespace

RadarCoordinateTable::RadarCoordinateTable(double latitude,
                                           double longitude,
                                           float  gateSize) :
    latitude_ {latitude}, longitude_ {longitude}, gateSize_ {gateSize}
{
}

const std::vector<float>&
RadarCoordinateTable::coordinates(common::RadialSize radialSize,
                                  bool               smoothingEnabled)
{
   EnsureCoordinatesInitialized(radialSize, smoothingEnabled);

   switch (radialSize)
   {
   case common::RadialSize::_0_5Degree:
      if (smoothingEnabled)
      {
         return coordinates0_5DegreeSmooth_;
      }
      return coordinates0_5Degree_;

   case common::RadialSize::_1Degree:
      if (smoothingEnabled)
      {
         return coordinates1DegreeSmooth_;
      }
      return coordinates1Degree_;

   default:
      throw std::invalid_argument("Invalid radial size");
   }
}

void RadarCoordinateTable::CalculateCoordinates(
   uint32_t                     radialCount,
   units::angle::degrees<float> radialAngle,
   units::angle::degrees<float> angleOffset,
   float                        gateRangeOffset,
   std::vector<float>&          outputCoordinates)
{
   const GeographicLib::Geodesic& geodesic(
      util::GeographicLib::DefaultGeodesic());

   const auto   radials = boost::irange<uint32_t>(0, radialCount);
   const double radarLatitude {latitude_};
   const double radarLongitude {longitude_};
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
               (static_cast<float>(gate) + gateRangeOffset) * gateSize_;
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

void RadarCoordinateTable::EnsureCoordinatesInitialized(
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
      coordinateCount   = kNumCoordinates0_5Degree_;
      radialCount       = kNumRadials0_5Degree_;
      radialAngle       = units::angle::degrees<float> {kRadialStepDegrees0_5_};
      angleOffset       = units::angle::degrees<float> {
         smoothingEnabled ? kSmoothingRadialOffsetDegrees0_5_ : 0.0F};
      gateRangeOffset = smoothingEnabled ? kSmoothingGateRangeOffset_ :
                                           kNoSmoothingGateRangeOffset_;
      timerName       = smoothingEnabled ? "Coordinates (0.5 degree smooth)" :
                                           "Coordinates (0.5 degree)";
      break;

   case common::RadialSize::_1Degree:
      targetCoordinates =
         smoothingEnabled ? &coordinates1DegreeSmooth_ : &coordinates1Degree_;
      coordinateCount = kNumCoordinates1Degree_;
      radialCount     = kNumRadials1Degree_;
      radialAngle     = units::angle::degrees<float> {1.0f};
      angleOffset     = units::angle::degrees<float> {
         smoothingEnabled ? kSmoothingRadialOffsetDegrees1_0_ : 0.0F};
      gateRangeOffset = smoothingEnabled ? kSmoothingGateRangeOffset_ :
                                           kNoSmoothingGateRangeOffset_;
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

} // namespace scwx::qt::manager
