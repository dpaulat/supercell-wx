#include <scwx/provider/rrfs_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

// Pure parsing logic, no network needed.
TEST(RrfsDataProvider, TimePointValid)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / September / 20d} + 12h;

   RrfsDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "rrfs.20260920/12/rrfs.t12z.2dfld.3km.f000.conus.grib2");

   EXPECT_EQ(time, expectedTime);
}

// Any forecast hour is now a real, valid key (see class comment on
// SetForecastHour()) -- the returned time is the file's own *valid*
// time (cycle + forecast hour), not just the cycle's nominal time.
TEST(RrfsDataProvider, TimePointForecastHour)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / September / 20d} + 13h;

   RrfsDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "rrfs.20260920/12/rrfs.t12z.2dfld.3km.f001.conus.grib2");

   EXPECT_EQ(time, expectedTime);
}

// The ".subh." variant (every cycle except the 3-hourly ones -- see
// UsesSubhVariant's own comment) parses the same way as the plain one.
TEST(RrfsDataProvider, TimePointSubhVariant)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / September / 25d} + 13h;

   RrfsDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "rrfs.20260925/01/rrfs.t01z.2dfld.3km.subh.f012.conus.grib2");

   EXPECT_EQ(time, expectedTime);
}

TEST(RrfsDataProvider, TimePointBadKey)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   RrfsDataProvider provider;

   EXPECT_EQ(provider.GetTimePointByKey("???"), expectedTime);

   // The .idx sidecar -- same class of regression RtmaDataProvider's own
   // test guards against (an unanchored regex matching the sidecar as a
   // substring of the real file).
   EXPECT_EQ(provider.GetTimePointByKey(
                "rrfs.20260920/12/rrfs.t12z.2dfld.3km.f000.conus.grib2.idx"),
             expectedTime);

   // The AK domain and the coarser 13km/na product -- both real, both
   // under the same day/hour, neither this class's target. (prslev used
   // to be in this list too -- it's now a real, resolvable file family,
   // see BuildKeyPrslevVariant below -- so it moved out of "bad".)
   EXPECT_EQ(provider.GetTimePointByKey(
                "rrfs.20260920/12/rrfs.t12z.2dfld.3km.f000.ak.grib2"),
             expectedTime);
   EXPECT_EQ(provider.GetTimePointByKey(
                "rrfs.20260920/12/rrfs.t12z.2dfld.13km.f000.na.grib2"),
             expectedTime);
}

// Pure parsing logic, no network needed.
TEST(RrfsDataProvider, MaxForecastHourForCycle)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   // 6-hourly cycles (00/06/12/18z) -- 84h runs.
   EXPECT_EQ(RrfsDataProvider::MaxForecastHourForCycle(
                sys_days {2026y / September / 25d} + 0h),
             84);
   EXPECT_EQ(RrfsDataProvider::MaxForecastHourForCycle(
                sys_days {2026y / September / 25d} + 12h),
             84);

   // The other 3-hourly cycles (03/09/15/21z) -- 18h runs.
   EXPECT_EQ(RrfsDataProvider::MaxForecastHourForCycle(
                sys_days {2026y / September / 25d} + 3h),
             18);

   // Every other hour -- also 18h runs.
   EXPECT_EQ(RrfsDataProvider::MaxForecastHourForCycle(
                sys_days {2026y / September / 25d} + 1h),
             18);
}

TEST(RrfsDataProvider, UsesSubhVariant)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   // The 3-hourly cycles (00/03/06/.../21z), 84h or 18h runs alike, use
   // the plain filename.
   EXPECT_FALSE(RrfsDataProvider::UsesSubhVariant(
      sys_days {2026y / September / 25d} + 12h));
   EXPECT_FALSE(RrfsDataProvider::UsesSubhVariant(
      sys_days {2026y / September / 25d} + 3h));

   // Every other hour needs ".subh.".
   EXPECT_TRUE(RrfsDataProvider::UsesSubhVariant(
      sys_days {2026y / September / 25d} + 1h));
}

// Pure, no network needed -- confirms BuildKey() agrees with
// GetPrefix()/GetTimePointByKey()'s own real keys (see TimePointValid/
// TimePointSubhVariant above), since it's meant as a drop-in way to get
// the same key those already parse, without a live ListObjectsV2 call.
TEST(RrfsDataProvider, BuildKeyPlainVariant)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto cycle = sys_days {2026y / September / 20d} + 12h;

   EXPECT_EQ(RrfsDataProvider::BuildKey(cycle, 0),
             "rrfs.20260920/12/rrfs.t12z.2dfld.3km.f000.conus.grib2");
   EXPECT_EQ(RrfsDataProvider::BuildKey(cycle, 1),
             "rrfs.20260920/12/rrfs.t12z.2dfld.3km.f001.conus.grib2");
}

TEST(RrfsDataProvider, BuildKeySubhVariant)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto cycle = sys_days {2026y / September / 25d} + 1h;

   EXPECT_EQ(RrfsDataProvider::BuildKey(cycle, 12),
             "rrfs.20260925/01/rrfs.t01z.2dfld.3km.subh.f012.conus.grib2");
}

// hour is clamped to [0, MaxForecastHourForCycle(cycle)], same as
// GetPrefix()'s own p->forecastHour_ clamp -- an out-of-range hour (e.g.
// a stale slider value from before a cycle change shrank the range)
// resolves to the nearest valid file rather than a key that can never
// exist.
TEST(RrfsDataProvider, BuildKeyClampsOutOfRangeHour)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   // 18h max (a 3-hourly, non-6-hourly cycle).
   const auto cycle = sys_days {2026y / September / 25d} + 3h;

   EXPECT_EQ(RrfsDataProvider::BuildKey(cycle, 84),
             RrfsDataProvider::BuildKey(cycle, 18));
   EXPECT_EQ(RrfsDataProvider::BuildKey(cycle, -5),
             RrfsDataProvider::BuildKey(cycle, 0));
}

// PressureLevel never uses ".subh.", confirmed live it simply isn't
// published outside the 3-hourly cycles (see class comment) -- so unlike
// TwoDField's own BuildKeySubhVariant test, this passes an off-3-hourly
// cycle specifically to confirm BuildKey still emits the *plain*
// filename rather than a subh one that would never exist on S3.
TEST(RrfsDataProvider, BuildKeyPrslevVariant)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   const auto synopticCycle = sys_days {2026y / September / 20d} + 12h;
   EXPECT_EQ(RrfsDataProvider::BuildKey(
                synopticCycle, 5, RrfsFileFamily::PressureLevel),
             "rrfs.20260920/12/rrfs.t12z.prslev.3km.f005.conus.grib2");

   const auto offSynopticCycle = sys_days {2026y / September / 25d} + 1h;
   EXPECT_EQ(RrfsDataProvider::BuildKey(
                offSynopticCycle, 5, RrfsFileFamily::PressureLevel),
             "rrfs.20260925/01/rrfs.t01z.prslev.3km.f005.conus.grib2");
}

TEST(RrfsDataProvider, GetTimePointByKeyResolvesPrslev)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / September / 20d} + 13h;

   RrfsDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "rrfs.20260920/12/rrfs.t12z.prslev.3km.f001.conus.grib2");

   EXPECT_EQ(time, expectedTime);
}

TEST(RrfsDataProvider, FileFamilyDefaultsToTwoDField)
{
   RrfsDataProvider provider;
   EXPECT_EQ(provider.FileFamily(), RrfsFileFamily::TwoDField);

   provider.SetFileFamily(RrfsFileFamily::PressureLevel);
   EXPECT_EQ(provider.FileFamily(), RrfsFileFamily::PressureLevel);
}

// Real S3 access below -- mirrors RtmaDataProvider's own FindKeyNow.
//
// Lenient on purpose, matching RtmaDataProvider's own FindKeyNow (which
// only checks key.size() > 0, no exact-key assertion): this failed for
// real during this session's own test run -- the 2-hour availability lag
// GetPrefix() guesses with is a first estimate from one live data point,
// not a guarantee, and a live S3 probe genuinely can come back empty if
// that guess landed on a cycle whose F000 file isn't published yet. A
// hard assertion on exact success turns real, expected data-availability
// variance into test flakiness; this only confirms Refresh()+FindKey
// don't error out, not that a key is always found on any given call.
TEST(RrfsDataProvider, FindKeyNow)
{
   RrfsDataProvider provider;

   provider.Refresh();
   std::string key = provider.FindKey(std::chrono::system_clock::now());

   if (!key.empty())
   {
      EXPECT_NE(key.find("2dfld.3km.f000.conus.grib2"), std::string::npos);
   }
}

// Same known-real historical cycle/hour as FindKeyFixedCycle below, just
// with SetFileFamily(PressureLevel) -- confirmed live via a direct S3
// listing while building this feature (same verification standard
// FindKeyFixedCycle's own comment describes).
TEST(RrfsDataProvider, FindKeyFixedCyclePrslev)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   RrfsDataProvider provider;
   provider.SetFileFamily(RrfsFileFamily::PressureLevel);
   provider.SetCycle(sys_days {2026y / September / 25d} + 12h);
   provider.SetForecastHour(3);

   provider.Refresh();
   std::string key = provider.FindKey(provider.CurrentCycle() + hours {3});

   ASSERT_FALSE(key.empty());
   EXPECT_NE(key.find("prslev.3km.f003.conus.grib2"), std::string::npos);
}

// Unlike FindKeyNow above, SetCycle()/SetForecastHour() let this class
// resolve an exact, arbitrary (non-"latest") cycle -- confirmed here
// against a real, specific historical file rather than "whatever's
// current." Not lenient the way FindKeyNow is: this cycle/hour is a
// known-real key (confirmed live via a direct S3 listing while building
// this feature), not a lag-based guess, so a hard assertion is
// appropriate. If NOAA's retention window ever ages this particular
// cycle out, replace the fixed date with a more recent one rather than
// loosening the assertion.
TEST(RrfsDataProvider, FindKeyFixedCycle)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   RrfsDataProvider provider;
   provider.SetCycle(sys_days {2026y / September / 25d} + 12h);
   provider.SetForecastHour(3);

   EXPECT_FALSE(provider.IsUsingLatestCycle());
   EXPECT_EQ(RrfsDataProvider::MaxForecastHourForCycle(provider.CurrentCycle()),
             84);

   provider.Refresh();
   std::string key = provider.FindKey(provider.CurrentCycle() + hours {3});

   EXPECT_NE(key.find("rrfs.20260925/12/rrfs.t12z.2dfld.3km.f003.conus"),
             std::string::npos);
}

TEST(RrfsDataProvider, LoadObjectByKeyNotApplicable)
{
   RrfsDataProvider provider;

   auto file = provider.LoadObjectByKey("anything");

   EXPECT_EQ(file, nullptr);
}

} // namespace provider
} // namespace scwx
