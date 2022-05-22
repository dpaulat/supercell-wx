#include <scwx/provider/aws_level2_data_provider.hpp>

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
   AwsLevel2DataProvider provider("KLSX");

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
}

TEST(AwsLevel2DataProvider, Refresh)
{
   AwsLevel2DataProvider provider("KLSX");

   provider.Refresh();

   EXPECT_GT(provider.cache_size(), 0);
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
