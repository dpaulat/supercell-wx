#include <scwx/qt/config/radar_site.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace config
{

static const std::string DEFAULT_RADAR_SITE_FILE =
   ":/res/config/radar_sites.json";

TEST(RadarSite, DefaultConfig)
{
   size_t numSites = RadarSite::ReadConfig(DEFAULT_RADAR_SITE_FILE);

   ASSERT_GT(numSites, 0);
   EXPECT_EQ(numSites, 204);

   std::shared_ptr<RadarSite> radarSite = RadarSite::Get("KLSX");

   ASSERT_NE(radarSite, nullptr);

   EXPECT_EQ(radarSite->type(), "wsr88d");
   EXPECT_EQ(radarSite->country(), "USA");
   EXPECT_EQ(radarSite->state(), "MO");
   EXPECT_EQ(radarSite->place(), "St. Louis");
   EXPECT_DOUBLE_EQ(radarSite->latitude(), 38.6986863);
   EXPECT_DOUBLE_EQ(radarSite->longitude(), -90.682877);
}

} // namespace config
} // namespace qt
} // namespace scwx
