#include <scwx/provider/aws_level2_data_provider.hpp>

#include <tuple>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

TEST(AwsLevel2DataProvider, FindKeyFixed)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto date = sys_days {2021y / May / 27d};
   const auto time = date + 17h + 59min;

   AwsLevel2DataProvider provider("KLSX");

   provider.ListObjects(date);
   std::string key = provider.FindKey(time);

   EXPECT_EQ(key, "2021/05/27/KLSX/KLSX20210527_175717_V06");
}

TEST(AwsLevel2DataProvider, FindKeyNow)
{
   AwsLevel2DataProvider provider("KILX");

   provider.Refresh();
   std::string key = provider.FindKey(std::chrono::system_clock::now());

   EXPECT_GT(key.size(), 0);
}

TEST(AwsLevel2DataProvider, LoadObjectByKey)
{
   const std::string key = "2022/04/21/KLSX/KLSX20220421_160055_V06";

   AwsLevel2DataProvider provider("KLSX");

   auto file = provider.LoadObjectByKey(key);

   EXPECT_NE(file, nullptr);
   ASSERT_NE(file, nullptr);
   EXPECT_EQ(file->filename(), "KLSX20220421_160055_V06");
   EXPECT_TRUE(file->has_file_data());
}

TEST(AwsLevel2DataProvider, Prune)
{
   using namespace std::chrono;

   AwsLevel2DataProvider provider("KEAX");

   const auto today     = floor<days>(system_clock::now());
   const auto yesterday = today - days {1};
   auto       date      = today + days {1};

   // Skip empty days (radar outages) so prune still sees 15 populated dates
   // and FindKey is queried on a date that was actually inserted into the
   // cache.
   for (size_t i = 0; i < 15;)
   {
      date -= days {1};
      ASSERT_GE(date, today - days {60})
         << "Not enough KEAX Level 2 objects in lookback window to exercise "
            "prune";

      const size_t totalObjects = std::get<2>(provider.ListObjects(date));

      // Expect the cache size to be under the prune threshold
      EXPECT_LE(provider.cache_size(), 2500);

      if (totalObjects > 0)
      {
         ++i;
      }
   }

   std::string key  = provider.FindLatestKey();
   auto        time = provider.GetTimePointByKey(key);

   // Expect the most recent key to be after midnight yesterday (was not pruned)
   EXPECT_GT(time, yesterday);

   key  = provider.FindKey(date + 1h);
   time = provider.GetTimePointByKey(key);

   // Expect the oldest key to be on the most recently listed date
   EXPECT_LT(time, date + days {1});
}

TEST(AwsLevel2DataProvider, Refresh)
{
   AwsLevel2DataProvider provider("KILX");

   auto [newObjects, totalObjects] = provider.Refresh();

   EXPECT_GT(newObjects, 0);
   EXPECT_GT(totalObjects, 0);
   EXPECT_GT(provider.cache_size(), 0);
   EXPECT_EQ(newObjects, totalObjects);
}

TEST(AwsLevel2DataProvider, TimePointValid)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime =
      sys_days {2022y / April / 30d} + 17h + 27min + 34s;

   auto time = AwsLevel2DataProvider::GetTimePointFromKey(
      "2022/04/30/KLSX/KLSX20220430_172734_V06.gz");

   EXPECT_EQ(time, expectedTime);
}

TEST(AwsLevel2DataProvider, TimePointInvalid)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   auto time = AwsLevel2DataProvider::GetTimePointFromKey(
      "2022/04/30/KLSX/KLSX20220430-172734_V06.gz");

   EXPECT_EQ(time, expectedTime);
}

TEST(AwsLevel2DataProvider, TimePointBadKey)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   auto time = AwsLevel2DataProvider::GetTimePointFromKey("???");

   EXPECT_EQ(time, expectedTime);
}

} // namespace provider
} // namespace scwx
