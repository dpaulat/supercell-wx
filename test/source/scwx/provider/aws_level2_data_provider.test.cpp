#include <scwx/provider/aws_level2_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

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

   // TODO: Check object count
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
