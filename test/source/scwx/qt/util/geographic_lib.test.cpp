#include <scwx/qt/util/geographic_lib.hpp>

#include <gtest/gtest.h>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

namespace scwx
{
namespace util
{

std::vector<common::Coordinate> area = {
   common::Coordinate(37.0193692, -91.8778413),
   common::Coordinate(36.9719180, -91.3006973),
   common::Coordinate(36.7270831, -91.6815753),
};

TEST(geographic_lib, area_in_range_inside)
{
   auto inside = common::Coordinate(36.9241584, -91.6425933);
   bool value;

   // inside is always true
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, inside, units::length::meters<double>(0));
   EXPECT_EQ(value, true);
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, inside, units::length::meters<double>(1e6));
   EXPECT_EQ(value, true);
}

TEST(geographic_lib, area_in_range_near)
{
   auto near = common::Coordinate(36.8009181, -91.3922700);
   bool value;

   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, near, units::length::meters<double>(9000));
   EXPECT_EQ(value, false);
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, near, units::length::meters<double>(10100));
   EXPECT_EQ(value, true);
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, near, units::length::meters<double>(1e6));
   EXPECT_EQ(value, true);
}

TEST(geographic_lib, area_in_range_far)
{
   auto far = common::Coordinate(37.6481966, -94.2163834);
   bool value;

   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, far, units::length::meters<double>(9000));
   EXPECT_EQ(value, false);
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, far, units::length::meters<double>(10100));
   EXPECT_EQ(value, false);
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, far, units::length::meters<double>(100e3));
   EXPECT_EQ(value, false);
   value = scwx::qt::util::GeographicLib::AreaInRangeOfPoint(
      area, far, units::length::meters<double>(300e3));
   EXPECT_EQ(value, true);
}

TEST(geographic_lib, radar_beam_altitude_at_radar)
{
   const units::length::meters<double> radarHeight {914.4}; // 3000 ft
   const auto                          altitude =
      scwx::qt::util::GeographicLib::GetRadarBeamAltititude(
         units::length::meters<double> {0.0},
         units::angle::degrees<double> {0.5},
         radarHeight);

   EXPECT_NEAR(altitude.value(), radarHeight.value(), 0.01);
}

TEST(geographic_lib, radar_beam_altitude_arl_near_radar)
{
   const units::length::meters<double> radarHeight {3048.0}; // ~10000 ft
   const units::length::meters<double> range {1000.0};
   const auto                          altitudeAmsl =
      scwx::qt::util::GeographicLib::GetRadarBeamAltititude(
         range, units::angle::degrees<double> {0.5}, radarHeight);
   const auto altitudeArl = altitudeAmsl - radarHeight;

   // Close to the radar, ARL should be much smaller than AMSL
   EXPECT_LT(altitudeArl.value(), 50.0);
   EXPECT_GT(altitudeArl.value(), 0.0);
   EXPECT_NEAR(altitudeAmsl.value(),
               radarHeight.value() + altitudeArl.value(),
               0.01);
}

TEST(geographic_lib, radar_beam_altitude_amsl_includes_site_height)
{
   const units::length::meters<double> lowSite {100.0};
   const units::length::meters<double> highSite {3000.0};
   const units::length::meters<double> range {5000.0};
   const units::angle::degrees<double> elevation {0.5};

   const auto lowAltitude =
      scwx::qt::util::GeographicLib::GetRadarBeamAltititude(
         range, elevation, lowSite);
   const auto highAltitude =
      scwx::qt::util::GeographicLib::GetRadarBeamAltititude(
         range, elevation, highSite);

   EXPECT_NEAR(highAltitude.value() - lowAltitude.value(),
               highSite.value() - lowSite.value(),
               1.0);
}

} // namespace util
} // namespace scwx
