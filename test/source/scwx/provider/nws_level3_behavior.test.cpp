#include <scwx/provider/nws_level3_behavior.hpp>

#include <gtest/gtest.h>

namespace scwx::provider
{

static const char* kNwsLevel3BaseUri =
   "https://tgftp.nws.noaa.gov/SL.us008001/DF.of/DC.radar/";

TEST(NwsLevel3BehaviorTest, GetFileUrl)
{
   const NwsLevel3Behavior behavior(kNwsLevel3BaseUri, "KLSX", "N0Q");

   EXPECT_EQ(behavior.GetFileUrl("sn.0001"),
             "https://tgftp.nws.noaa.gov/SL.us008001/DF.of/DC.radar/DS.p94r0/"
             "SI.klsx/sn.0001");
}

TEST(NwsLevel3BehaviorTest, InvalidProductListObjects)
{
   NwsLevel3Behavior behavior(kNwsLevel3BaseUri, "KLSX", "???");

   const auto objects =
      behavior.ListObjects(std::chrono::system_clock::now()).second;

   EXPECT_TRUE(objects.empty());
}

TEST(NwsLevel3BehaviorTest, DateArchiveNotAvailable)
{
   const NwsLevel3Behavior behavior(kNwsLevel3BaseUri, "KLSX", "N0Q");

   EXPECT_FALSE(behavior.date_archive_available());
}

TEST(NwsLevel3BehaviorTest, ListObjects)
{
   NwsLevel3Behavior behavior(kNwsLevel3BaseUri, "KLSX", "N0Q");

   const auto objects =
      behavior.ListObjects(std::chrono::system_clock::now()).second;

   if (objects.empty())
   {
      GTEST_SKIP() << "Network unavailable or listing empty";
   }

   EXPECT_GT(objects.size(), 0u);

   for (const auto& key : objects)
   {
      EXPECT_NE(key, "sn.last");
      EXPECT_TRUE(key.starts_with("sn."));
   }
}

TEST(NwsLevel3BehaviorTest, GetTimePointByKey)
{
   NwsLevel3Behavior behavior(kNwsLevel3BaseUri, "KLSX", "N0Q");

   const auto objects =
      behavior.ListObjects(std::chrono::system_clock::now()).second;

   if (objects.empty())
   {
      GTEST_SKIP() << "Network unavailable or listing empty";
   }

   const auto time = behavior.GetTimePointByKey(objects.front());
   EXPECT_NE(time, std::chrono::system_clock::time_point {});
}

} // namespace scwx::provider
