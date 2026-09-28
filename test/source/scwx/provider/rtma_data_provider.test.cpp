#include <scwx/provider/rtma_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

// Pure parsing logic, no network needed. Unlike MRMS (every listed key is
// expected to match), an RTMA date folder also holds .idx/.gif siblings
// and other RTMA products (see GetTimePointByKey's comment) -- those are
// exercised here too, confirming they correctly fall through to the
// default time_point{} rather than false-matching.
TEST(RtmaDataProvider, TimePointValid)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2024y / January / 15d} + 0h;

   RtmaDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "rtma2p5.20240115/rtma2p5.t00z.2dvaranl_ndfd.grb2_wexp");

   EXPECT_EQ(time, expectedTime);
}

TEST(RtmaDataProvider, TimePointBadKey)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   RtmaDataProvider provider;

   EXPECT_EQ(provider.GetTimePointByKey("???"), expectedTime);
   EXPECT_EQ(provider.GetTimePointByKey("rtma2p5.20240115/ls-l"), expectedTime);
   EXPECT_EQ(
      provider.GetTimePointByKey("rtma2p5.20240115/pcprtma.2024011500.gif"),
      expectedTime);

   // Regression: the real analysis file's own .idx sidecar used to match
   // too (regex_search finding "...grb2_wexp" as a substring of
   // "...grb2_wexp.idx"), silently clobbering the real file's entry in
   // AwsNexradDataProvider's time-keyed map since both parsed to the same
   // time point -- confirmed live via FindLatestKey() returning an .idx
   // path instead of the actual GRIB2 data. Anchoring the regex with $
   // fixed it; this guards against it coming back.
   EXPECT_EQ(provider.GetTimePointByKey(
                "rtma2p5.20240115/rtma2p5.t00z.2dvaranl_ndfd.grb2_wexp.idx"),
             expectedTime);
}

// Real S3 access below -- mirrors MrmsDataProvider's own tests, against a
// date confirmed (this session) to have a full 24-hour set of files.
TEST(RtmaDataProvider, FindKeyFixed)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto date = sys_days {2024y / January / 15d};
   const auto time = date + 12h;

   RtmaDataProvider provider;

   provider.ListObjects(date);
   std::string key = provider.FindKey(time);

   EXPECT_NE(key.find("2dvaranl_ndfd.grb2_wexp"), std::string::npos);
   EXPECT_NE(key.find("t12z"), std::string::npos);
   EXPECT_NE(key.find("20240115"), std::string::npos);
}

TEST(RtmaDataProvider, FindKeyNow)
{
   RtmaDataProvider provider;

   provider.Refresh();
   std::string key = provider.FindKey(std::chrono::system_clock::now());

   EXPECT_GT(key.size(), 0);
}

TEST(RtmaDataProvider, GetTimePointsByDate)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto date     = sys_days {2024y / January / 15d};
   const auto tomorrow = date + days {1};

   RtmaDataProvider provider;

   auto timePoints = provider.GetTimePointsByDate(date, true);

   // 24 hourly analyses expected -- confirmed via a real bucket listing
   // during this session, not just "at least one" like MrmsDataProvider's
   // equivalent test (MRMS's ~2 minute cadence makes an exact count
   // fragile; RTMA's fixed hourly cadence on a settled archive date does
   // not).
   EXPECT_EQ(timePoints.size(), 24);
   for (auto timePoint : timePoints)
   {
      EXPECT_GE(timePoint, date);
      EXPECT_LT(timePoint, tomorrow);
   }
}

// LoadObjectByKey/LoadObjectByTime are stubbed (see rtma_data_provider.hpp
// for why -- wrong return type for GRIB2), confirm that explicitly rather
// than leaving it unverified.
TEST(RtmaDataProvider, LoadObjectByKeyNotApplicable)
{
   RtmaDataProvider provider;

   auto file = provider.LoadObjectByKey("anything");

   EXPECT_EQ(file, nullptr);
}

} // namespace provider
} // namespace scwx
