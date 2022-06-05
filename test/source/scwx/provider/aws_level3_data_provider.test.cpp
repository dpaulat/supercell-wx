#include <scwx/provider/aws_level3_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

TEST(AwsLevel3DataProvider, FindKeyFixed)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto date = sys_days {2021y / May / 27d};
   const auto time = date + 17h + 59min;

   AwsLevel3DataProvider provider("KLSX", "N0Q");

   provider.ListObjects(date);
   std::string key = provider.FindKey(time);

   EXPECT_EQ(key, "LSX_N0Q_2021_05_27_17_57_17");
}

TEST(AwsLevel3DataProvider, FindKeyNow)
{
   AwsLevel3DataProvider provider("KLSX", "N0B");

   provider.Refresh();
   std::string key = provider.FindKey(std::chrono::system_clock::now());

   EXPECT_GT(key.size(), 0);
}

TEST(AwsLevel3DataProvider, LoadObjectByKey)
{
   const std::string key = "LSX_N0Q_2021_05_27_17_57_17";

   AwsLevel3DataProvider provider("KLSX", "N0Q");

   auto file = provider.LoadObjectByKey(key);

   EXPECT_NE(file, nullptr);
}

TEST(AwsLevel3DataProvider, Refresh)
{
   AwsLevel3DataProvider provider("KLSX", "N0B");

   auto [newObjects, totalObjects] = provider.Refresh();

   EXPECT_GT(newObjects, 0);
   EXPECT_GT(totalObjects, 0);
   EXPECT_GT(provider.cache_size(), 0);
   EXPECT_EQ(newObjects, totalObjects);
}

TEST(AwsLevel3DataProvider, TimePointValid)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime =
      sys_days {2022y / April / 30d} + 17h + 27min + 34s;

   auto time =
      AwsLevel3DataProvider::GetTimePointFromKey("LSX_N0Q_2022_04_30_17_27_34");

   EXPECT_EQ(time, expectedTime);
}

TEST(AwsLevel3DataProvider, TimePointInvalid)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   auto time =
      AwsLevel3DataProvider::GetTimePointFromKey("LSX_N0Q_2022-04-30_17-27-34");

   EXPECT_EQ(time, expectedTime);
}

TEST(AwsLevel3DataProvider, TimePointBadKey)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   auto time = AwsLevel3DataProvider::GetTimePointFromKey("???");

   EXPECT_EQ(time, expectedTime);
}

} // namespace provider
} // namespace scwx
