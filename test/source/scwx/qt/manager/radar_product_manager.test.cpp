#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/constants.hpp>

#include <array>
#include <cstdint>
#include <utility>

#include <gtest/gtest.h>

namespace scwx::qt::manager
{

namespace
{

void ValidateCoordinateSamples(const RadarProductManager&   manager,
                               common::RadialSize           radialSize,
                               bool                         smoothingEnabled,
                               std::uint32_t                radialCount,
                               units::angle::degrees<float> radialAngle,
                               units::angle::degrees<float> angleOffset,
                               float                        gateRangeOffset)
{
   const auto radarSite = manager.radar_site();
   ASSERT_NE(radarSite, nullptr);

   const auto& coordinates = manager.coordinates(radialSize, smoothingEnabled);
   ASSERT_FALSE(coordinates.empty());

   const auto& geodesic = util::GeographicLib::DefaultGeodesic();
   const float gateSize = manager.gate_size();

   const std::array<std::pair<std::uint32_t, std::uint32_t>, 4> samples {
      std::pair<std::uint32_t, std::uint32_t> {0u, 0u},
      std::pair<std::uint32_t, std::uint32_t> {1u, 100u},
      std::pair<std::uint32_t, std::uint32_t> {radialCount / 2u, 500u},
      std::pair<std::uint32_t, std::uint32_t> {
         radialCount - 1u, common::MAX_DATA_MOMENT_GATES - 1u}};

   for (const auto& [radial, gate] : samples)
   {
      const float angle =
         static_cast<float>(radial) * radialAngle.value() + angleOffset.value();
      const float range =
         (static_cast<float>(gate) + gateRangeOffset) * gateSize;
      const std::size_t index =
         (static_cast<std::size_t>(radial) * common::MAX_DATA_MOMENT_GATES +
          static_cast<std::size_t>(gate)) *
         2u;

      double expectedLatitude  = 0.0;
      double expectedLongitude = 0.0;

      geodesic.Direct(radarSite->latitude(),
                      radarSite->longitude(),
                      angle,
                      range,
                      expectedLatitude,
                      expectedLongitude);

      EXPECT_NEAR(
         static_cast<double>(coordinates[index]), expectedLatitude, 1e-5);
      EXPECT_NEAR(
         static_cast<double>(coordinates[index + 1u]), expectedLongitude, 1e-5);
   }
}

} // namespace

// Spot-checks lazy-built tables against GeographicLib::Geodesic::Direct
// (reference).
TEST(RadarProductManager, CoordinateGenerationMatchesGeodesicReference)
{
   config::RadarSite::Initialize();
   const auto manager = RadarProductManager::Instance("KLSX");
   ASSERT_NE(manager, nullptr);

   manager->Initialize();

   ValidateCoordinateSamples(*manager,
                             common::RadialSize::_0_5Degree,
                             false,
                             common::MAX_0_5_DEGREE_RADIALS,
                             units::angle::degrees<float> {0.5f},
                             units::angle::degrees<float> {0.0f},
                             1.0f);
   ValidateCoordinateSamples(*manager,
                             common::RadialSize::_0_5Degree,
                             true,
                             common::MAX_0_5_DEGREE_RADIALS,
                             units::angle::degrees<float> {0.5f},
                             units::angle::degrees<float> {0.25f},
                             0.5f);
   ValidateCoordinateSamples(*manager,
                             common::RadialSize::_1Degree,
                             false,
                             common::MAX_1_DEGREE_RADIALS,
                             units::angle::degrees<float> {1.0f},
                             units::angle::degrees<float> {0.0f},
                             1.0f);
   ValidateCoordinateSamples(*manager,
                             common::RadialSize::_1Degree,
                             true,
                             common::MAX_1_DEGREE_RADIALS,
                             units::angle::degrees<float> {1.0f},
                             units::angle::degrees<float> {0.5f},
                             0.5f);
}

} // namespace scwx::qt::manager
