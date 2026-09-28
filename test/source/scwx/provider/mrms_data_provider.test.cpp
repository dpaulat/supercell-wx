#include <scwx/provider/mrms_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

static const std::string kMrmsProduct_ = "MergedReflectivityQCComposite_00.50";

// Pure parsing logic, no network needed -- exercises exactly what
// AwsNexradDataProvider::ListObjects calls per key during Refresh()/
// ListObjects(), which is what FindKey ultimately searches.
TEST(MrmsDataProvider, TimePointValid)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime =
      sys_days {2020y / October / 16d} + 5h + 34min + 35s;

   MrmsDataProvider provider(kMrmsProduct_);
   auto             time = provider.GetTimePointByKey(
      "CONUS/MergedReflectivityQCComposite_00.50/20201016/"
      "MRMS_MergedReflectivityQCComposite_00.50_20201016-053435.grib2.gz");

   EXPECT_EQ(time, expectedTime);
}

TEST(MrmsDataProvider, TimePointBadKey)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   MrmsDataProvider provider(kMrmsProduct_);
   auto             time = provider.GetTimePointByKey("???");

   EXPECT_EQ(time, expectedTime);
}

// Real S3 access below -- mirrors AwsLevel3DataProvider's FindKeyFixed/
// GetTimePointsByDate/Refresh tests, against a file confirmed to exist
// during this session's own manual testing (see grib-viewer-build-steps.md).
TEST(MrmsDataProvider, FindKeyFixed)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto date = sys_days {2020y / October / 16d};
   const auto time = date + 5h + 34min;

   MrmsDataProvider provider(kMrmsProduct_);

   provider.ListObjects(date);
   std::string key = provider.FindKey(time);

   EXPECT_NE(key.find("MergedReflectivityQCComposite"), std::string::npos);
   EXPECT_NE(key.find("20201016"), std::string::npos);
}

TEST(MrmsDataProvider, FindKeyNow)
{
   MrmsDataProvider provider(kMrmsProduct_);

   provider.Refresh();
   std::string key = provider.FindKey(std::chrono::system_clock::now());

   EXPECT_GT(key.size(), 0);
}

TEST(MrmsDataProvider, Refresh)
{
   MrmsDataProvider provider(kMrmsProduct_);

   auto [newObjects, totalObjects] = provider.Refresh();

   EXPECT_GT(newObjects, 0);
   EXPECT_GT(totalObjects, 0);
   EXPECT_GT(provider.cache_size(), 0);
   EXPECT_EQ(newObjects, totalObjects);
}

TEST(MrmsDataProvider, GetTimePointsByDate)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto date     = sys_days {2020y / October / 16d};
   const auto tomorrow = date + days {1};

   MrmsDataProvider provider(kMrmsProduct_);

   auto timePoints = provider.GetTimePointsByDate(date, true);

   EXPECT_GT(timePoints.size(), 0);
   for (auto timePoint : timePoints)
   {
      EXPECT_GE(timePoint, date);
      EXPECT_LT(timePoint, tomorrow);
   }
}

// LoadObjectByKey/LoadObjectByTime are stubbed (see mrms_data_provider.hpp
// for why -- wrong return type for GRIB2), confirm that explicitly rather
// than leaving it unverified.
TEST(MrmsDataProvider, LoadObjectByKeyNotApplicable)
{
   MrmsDataProvider provider(kMrmsProduct_);

   auto file = provider.LoadObjectByKey("anything");

   EXPECT_EQ(file, nullptr);
}

} // namespace provider
} // namespace scwx
