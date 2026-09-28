#include <scwx/qt/manager/grib_manager.hpp>
#include <scwx/qt/map/grib_frame_info.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <numeric>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace manager
{

// Real S3 access below (SetRrfsCycle()/SetRrfsForecastHour()/
// UseLatestRrfsCycle() all fetch immediately, see FetchRrfsSelection() in
// grib_manager.cpp) -- mirrors RrfsDataProvider's own FindKeyFixedCycle
// test, which this reuses the same known-real historical cycle from.
// GribManager::Instance() is a process-wide singleton keyed by category
// (see its own comment on why), so this is the one test file allowed to
// touch map::GribCategory::Rrfs through it.
TEST(GribManagerTest, RrfsForecastHourSelection)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   auto gribManager = GribManager::Instance(map::GribCategory::Rrfs);

   // Defaults, before any selection has been made.
   EXPECT_TRUE(gribManager->IsUsingLatestRrfsCycle());
   EXPECT_EQ(gribManager->RrfsForecastHour(), 0);

   // Forecast-hour selection alone doesn't fix the cycle -- "latest cycle,
   // hour 5" is a valid, still-auto-tracking combination (see
   // SetRrfsForecastHour()'s own doc in grib_manager.hpp).
   gribManager->SetRrfsForecastHour(5);
   EXPECT_EQ(gribManager->RrfsForecastHour(), 5);
   EXPECT_TRUE(gribManager->IsUsingLatestRrfsCycle());

   // A fixed cycle -- same known-real historical cycle
   // RrfsDataProvider.FindKeyFixedCycle uses, confirmed live while this
   // feature was built. 12z is a 6-hourly cycle, so 84h.
   const auto fixedCycle = sys_days {2026y / September / 25d} + 12h;
   gribManager->SetRrfsCycle(fixedCycle);
   EXPECT_FALSE(gribManager->IsUsingLatestRrfsCycle());
   EXPECT_EQ(gribManager->CurrentRrfsCycle(), fixedCycle);
   EXPECT_EQ(gribManager->MaxRrfsForecastHour(), 84);
   // The forecast-hour selection from above persists across a cycle
   // change -- SetRrfsCycle() doesn't reset it.
   EXPECT_EQ(gribManager->RrfsForecastHour(), 5);

   // A 3-hourly (non-6-hourly) cycle -- 18h.
   gribManager->SetRrfsCycle(sys_days {2026y / September / 25d} + 15h);
   EXPECT_EQ(gribManager->MaxRrfsForecastHour(), 18);

   gribManager->UseLatestRrfsCycle();
   EXPECT_TRUE(gribManager->IsUsingLatestRrfsCycle());
}

// Real S3 access + a real ~580MB fetch and decode below -- confirms the
// prslev file-family axis (see provider::RrfsFileFamily) actually
// reaches a correctly-targeted real decode end to end, not just that it
// compiles. Same known-real historical cycle/hour as
// RrfsForecastHourSelection above (and RrfsDataProvider's own
// FindKeyFixedCyclePrslev, confirmed live while building this feature).
TEST(GribManagerTest, PrslevProductDecodesRealFile)
{
   using namespace std::chrono;
   using namespace std::chrono_literals;
   using sys_days = time_point<system_clock, days>;

   auto gribManager = GribManager::Instance(map::GribCategory::Rrfs);

   const auto names = gribManager->ProductNames();
   const auto it    = std::find(names.begin(), names.end(), "500mb Height");
   ASSERT_NE(it, names.end());
   const std::size_t productIndex =
      static_cast<std::size_t>(std::distance(names.begin(), it));

   gribManager->SetProductActive("500mb Height", true);
   gribManager->SetRrfsCycle(sys_days {2026y / September / 25d} + 12h);
   gribManager->SetRrfsForecastHour(3);

   // wxtest has its own, separate download cache from the real app (see
   // HodographManagerTest's own comment on this) -- a real cold ~580MB
   // fetch (prslev is larger than 2dfld's own ~320MB) plus decode runs on
   // a background thread; poll for the result rather than assuming
   // timing. ~150s of headroom, scaled up from HodographManagerTest's
   // own ~90s/320MB budget for this file's larger size.
   //
   // Checks the *decoded field's own mean*, not the frame header's
   // "validTime" -- a real trap found while building this test:
   // decode_grib's ProductInfoFromHandle reports a GRIB2 message's own
   // dataDate/dataTime, which for a forecast field is the *cycle's*
   // reference time, not (reference + forecast hour) -- so F000 and F003
   // of the same cycle report the identical "validTime" regardless, and
   // a validTime-based check can never actually distinguish them (it
   // passed once against a genuinely-stale F000 frame purely because
   // both said the same timestamp). 500mb height's own real mean
   // *does* differ meaningfully between these two specific hours
   // (confirmed live: 5834.78 at F000, 5837.13 at F003, on this exact
   // historical cycle) -- tight enough tolerance to tell them apart, but
   // real model data, not a synthetic marker value.
   const std::string framePath =
      map::GetGribFramePath(map::GribCategory::Rrfs, productIndex);
   constexpr double   kExpectedF3Mean = 5837.13;
   constexpr double   kTolerance      = 0.5;
   bool               found           = false;
   double             lastMean        = 0.0;
   std::vector<float> payload;

   for (int i = 0; i < 300 && !found; ++i)
   {
      std::ifstream in(framePath, std::ios::binary);
      if (in.is_open())
      {
         std::string header;
         std::getline(in, header);

         payload.assign(1905141, 0.0f);
         in.read(reinterpret_cast<char*>(payload.data()),
                 static_cast<std::streamsize>(payload.size() * sizeof(float)));

         if (in.good() || in.eof())
         {
            const double sum =
               std::accumulate(payload.begin(), payload.end(), 0.0);
            lastMean = sum / static_cast<double>(payload.size());

            if (std::abs(lastMean - kExpectedF3Mean) < kTolerance)
            {
               found = true;
               break;
            }
         }
      }
      std::this_thread::sleep_for(500ms);
   }

   EXPECT_TRUE(found) << "Last decoded mean seen: " << lastMean
                      << " (expected ~" << kExpectedF3Mean << ")";

   // Leave the shared singleton no worse than found, same discipline
   // RrfsForecastHourSelection's own final UseLatestRrfsCycle() call
   // uses -- this is the one test file allowed to touch
   // GribCategory::Rrfs through it (see that test's own comment), so
   // later tests in this binary shouldn't see this product still active.
   gribManager->SetProductActive("500mb Height", false);
   gribManager->UseLatestRrfsCycle();
}

// Confirms SHIP's own two-file dispatch (FetchShipSelection()/
// QueueShipInput()/ApplyShipIfReady()/ApplyShipDownload(), see their own
// docs) end to end against real data, not just that decode_grib's
// `--derived ship` mode compiles. Same known-real historical cycle/hour
// as PrslevProductDecodesRealFile above -- deliberately reuses it so
// both of SHIP's inputs (2dfld and prslev) are typically already cached
// from that test's own run, making this one a fast synchronous decode
// rather than two fresh multi-hundred-MB downloads; either way, the
// poll loop below doesn't assume which.
TEST(GribManagerTest, ShipProductDecodesRealFile)
{
   using namespace std::chrono;
   using namespace std::chrono_literals;
   using sys_days = time_point<system_clock, days>;

   auto gribManager = GribManager::Instance(map::GribCategory::Rrfs);

   const auto names = gribManager->ProductNames();
   const auto it    = std::find(names.begin(), names.end(), "SHIP");
   ASSERT_NE(it, names.end());
   const std::size_t productIndex =
      static_cast<std::size_t>(std::distance(names.begin(), it));

   gribManager->SetProductActive("SHIP", true);
   gribManager->SetRrfsCycle(sys_days {2026y / September / 25d} + 12h);
   gribManager->SetRrfsForecastHour(3);

   // Checks the decoded field's own mean, not header metadata -- same
   // lesson as PrslevProductDecodesRealFile's own comment on
   // decode_grib's validTime not being usable to distinguish forecast
   // hours. SHIP's real mean at this exact cycle/hour (verified live via
   // a direct decode_grib --derived ship CLI run while building this
   // feature): 0.019086.
   const std::string framePath =
      map::GetGribFramePath(map::GribCategory::Rrfs, productIndex);
   constexpr double   kExpectedMean = 0.019086;
   constexpr double   kTolerance    = 0.001;
   bool               found         = false;
   double             lastMean      = 0.0;
   std::vector<float> payload;

   for (int i = 0; i < 300 && !found; ++i)
   {
      std::ifstream in(framePath, std::ios::binary);
      if (in.is_open())
      {
         std::string header;
         std::getline(in, header);

         payload.assign(1905141, 0.0f);
         in.read(reinterpret_cast<char*>(payload.data()),
                 static_cast<std::streamsize>(payload.size() * sizeof(float)));

         if (in.good() || in.eof())
         {
            const double sum =
               std::accumulate(payload.begin(), payload.end(), 0.0);
            lastMean = sum / static_cast<double>(payload.size());

            if (std::abs(lastMean - kExpectedMean) < kTolerance)
            {
               found = true;
               break;
            }
         }
      }
      std::this_thread::sleep_for(500ms);
   }

   EXPECT_TRUE(found) << "Last decoded mean seen: " << lastMean
                      << " (expected ~" << kExpectedMean << ")";

   gribManager->SetProductActive("SHIP", false);
   gribManager->UseLatestRrfsCycle();
}

// SetRrfsCycle()/SetRrfsForecastHour()/UseLatestRrfsCycle() are refused
// (logged, not crashing) against a non-Rrfs instance -- Mrms/Rtma have no
// forecast-hour axis at all (see the class comment on these methods).
TEST(GribManagerTest, RrfsSelectionNoOpForOtherCategories)
{
   auto gribManager = GribManager::Instance(map::GribCategory::Mrms);

   EXPECT_TRUE(gribManager->IsUsingLatestRrfsCycle());
   EXPECT_EQ(gribManager->RrfsForecastHour(), 0);
   EXPECT_EQ(gribManager->MaxRrfsForecastHour(), 0);
   EXPECT_EQ(gribManager->CurrentRrfsCycle(),
             std::chrono::system_clock::time_point {});

   gribManager->SetRrfsForecastHour(5);
   EXPECT_EQ(gribManager->RrfsForecastHour(), 0);
}

} // namespace manager
} // namespace qt
} // namespace scwx
