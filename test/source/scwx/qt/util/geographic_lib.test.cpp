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


} // namespace util
} // namespace scwx
