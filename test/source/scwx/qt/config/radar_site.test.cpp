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

class RadarSiteTest : public testing::Test
{
protected:
   static size_t numSites_;

   static void SetUpTestSuite()
   {
      numSites_ = RadarSite::ReadConfig(DEFAULT_RADAR_SITE_FILE);
   }
};

size_t RadarSiteTest::numSites_ {0u};

TEST_F(RadarSiteTest, DefaultConfig)
{
   ASSERT_GT(numSites_, 0);
   EXPECT_EQ(numSites_, 204);

   std::shared_ptr<RadarSite> radarSite = RadarSite::Get("KLSX");

   ASSERT_NE(radarSite, nullptr);

   EXPECT_EQ(radarSite->type(), "wsr88d");
   EXPECT_EQ(radarSite->country(), "USA");
   EXPECT_EQ(radarSite->state(), "MO");
   EXPECT_EQ(radarSite->place(), "St. Louis");
   EXPECT_DOUBLE_EQ(radarSite->latitude(), 38.6986863);
   EXPECT_DOUBLE_EQ(radarSite->longitude(), -90.682877);
}

TEST_F(RadarSiteTest, FindNearest)
{
   ASSERT_GT(numSites_, 0);

   std::shared_ptr<RadarSite> nearest1 =
      RadarSite::FindNearest(46.591111, -112.020278); // Helena, MT
   std::shared_ptr<RadarSite> nearest2 =
      RadarSite::FindNearest(28.54, -81.38); // Orlando, FL
   std::shared_ptr<RadarSite> nearest3 =
      RadarSite::FindNearest(38.627222, -90.197778, "wsr88d"); // St Louis, MO
   std::shared_ptr<RadarSite> nearest4 =
      RadarSite::FindNearest(38.627222, -90.197778, "tdwr"); // St Louis, MO

   EXPECT_EQ(nearest1->id(), "KTFX");
   EXPECT_EQ(nearest2->id(), "TMCO");
   EXPECT_EQ(nearest3->id(), "KLSX");
   EXPECT_EQ(nearest4->id(), "TSTL");
}

} // namespace config
} // namespace qt
} // namespace scwx
