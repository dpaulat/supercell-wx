#include <scwx/provider/nbm_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

TEST(NbmDataProvider, TimePointValid)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / September / 25d} + 12h;

   NbmDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "blend.20260925/12/core/blend.t12z.core.f000.co.grib2");

   EXPECT_EQ(time, expectedTime);
}

TEST(NbmDataProvider, TimePointForecastHour)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / September / 25d} + 13h;

   NbmDataProvider provider;
   auto             time = provider.GetTimePointByKey(
      "blend.20260925/12/core/blend.t12z.core.f001.co.grib2");

   EXPECT_EQ(time, expectedTime);
}

TEST(NbmDataProvider, TimePointBadKey)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   NbmDataProvider provider;

   EXPECT_EQ(provider.GetTimePointByKey("???"), expectedTime);

   // The .idx sidecar -- same regression class RtmaDataProvider's own
   // test guards against (an unanchored regex matching it as a
   // substring of the real key).
   EXPECT_EQ(provider.GetTimePointByKey(
                "blend.20260925/12/core/blend.t12z.core.f000.co.grib2.idx"),
             expectedTime);

   // A non-CONUS domain -- real, but not this class's target.
   EXPECT_EQ(provider.GetTimePointByKey(
                "blend.20260925/12/core/blend.t12z.core.f000.ak.grib2"),
             expectedTime);
}

TEST(NbmDataProvider, MaxForecastHourForCycle)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto day = sys_days {2026y / September / 25d};

   // Extended (6-hourly) cycles: confirmed live out to F264.
   EXPECT_EQ(NbmDataProvider::MaxForecastHourForCycle(day + 0h), 264);
   EXPECT_EQ(NbmDataProvider::MaxForecastHourForCycle(day + 6h), 264);
   EXPECT_EQ(NbmDataProvider::MaxForecastHourForCycle(day + 12h), 264);
   EXPECT_EQ(NbmDataProvider::MaxForecastHourForCycle(day + 18h), 264);

   // Every other hourly cycle: conservative, always-available cutoff.
   EXPECT_EQ(NbmDataProvider::MaxForecastHourForCycle(day + 9h), 36);
   EXPECT_EQ(NbmDataProvider::MaxForecastHourForCycle(day + 15h), 36);
}

TEST(NbmDataProvider, UsesExtendedRange)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto day = sys_days {2026y / September / 25d};

   EXPECT_TRUE(NbmDataProvider::UsesExtendedRange(day + 0h));
   EXPECT_TRUE(NbmDataProvider::UsesExtendedRange(day + 18h));
   EXPECT_FALSE(NbmDataProvider::UsesExtendedRange(day + 9h));
   EXPECT_FALSE(NbmDataProvider::UsesExtendedRange(day + 23h));
}

TEST(NbmDataProvider, SnapForecastHourHourlyBand)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto cycle = sys_days {2026y / September / 25d} + 0h;

   // Within the hourly band (<=69), every hour is valid as-is -- except 0,
   // which NBM doesn't publish at all (confirmed live 2026-09-26: no
   // F000 file for the CONUS core product), so it's clamped up to 1.
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 0), 1);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 1), 1);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 45), 45);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 69), 69);
}

TEST(NbmDataProvider, SnapForecastHour3HourlyBand)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto cycle = sys_days {2026y / September / 25d} + 0h;

   // The real gap this test guards against: F070/F071 aren't published
   // (confirmed live 2026-09-26) -- the next real hour is F072.
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 70), 72);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 71), 72);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 72), 72);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 73), 75);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 189), 189);
}

TEST(NbmDataProvider, SnapForecastHour6HourlyBand)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto cycle = sys_days {2026y / September / 25d} + 0h;

   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 190), 192);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 191), 192);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 193), 198);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 264), 264);

   // Clamped to the real max even if asked for more.
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 999), 264);
}

TEST(NbmDataProvider, SnapForecastHourShortCycleIsAlwaysHourly)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto cycle = sys_days {2026y / September / 25d} + 9h;

   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 30), 30);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 36), 36);
   EXPECT_EQ(NbmDataProvider::SnapForecastHour(cycle, 100), 36);
}

TEST(NbmDataProvider, BuildKeyPlain)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto cycle = sys_days {2026y / September / 25d} + 12h;

   // hour 0 clamps up to 1 -- NBM has no F000 file at all (confirmed
   // live 2026-09-26).
   EXPECT_EQ(NbmDataProvider::BuildKey(cycle, 0),
            "blend.20260925/12/core/blend.t12z.core.f001.co.grib2");
   EXPECT_EQ(NbmDataProvider::BuildKey(cycle, 72),
            "blend.20260925/12/core/blend.t12z.core.f072.co.grib2");
}

TEST(NbmDataProvider, BuildKeyClampsOutOfRangeHour)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   // A short (non-extended) cycle clamps to 36, not the extended-cycle
   // max of 264.
   constexpr auto cycle = sys_days {2026y / September / 25d} + 9h;

   EXPECT_EQ(NbmDataProvider::BuildKey(cycle, 200),
            "blend.20260925/09/core/blend.t09z.core.f036.co.grib2");
}

TEST(NbmDataProvider, LoadObjectByKeyNotApplicable)
{
   NbmDataProvider provider;
   EXPECT_EQ(provider.LoadObjectByKey("blend.20260925/12/core/x"), nullptr);
}

} // namespace provider
} // namespace scwx
