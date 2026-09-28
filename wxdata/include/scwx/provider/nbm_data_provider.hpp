#pragma once

#include <scwx/provider/aws_nexrad_data_provider.hpp>

#include <chrono>
#include <string>

namespace scwx::provider
{

// Fourth GRIB2 source, alongside MRMS/RTMA/RRFS. Unlike any of those,
// NBM's own per-cycle file carries every field for the whole CONUS
// domain in one object (~160MB) -- too large to download whole per
// product the way RTMA/RRFS do. Every NBM product instead downloads
// exactly one field via AwsNexradDataProvider::DownloadGribMessageByIndex()
// (see FetchField() below and scwx::util::grib_idx), keyed by that
// field's own idx parameter/level/qualifier -- see
// manager::GribManager::kNbmProducts_.
//
// Real NBM S3 layout, confirmed live (2026-09-26):
// blend.YYYYMMDD/HH/core/blend.tHHz.core.fFFF.co.grib2 ("co" = CONUS;
// this class only ever targets that domain -- ak/hi/gu/pr also exist but
// aren't relevant to a CONUS radar viewer). Publishes hourly (00-23z),
// but forecast-hour availability differs by cycle hour -- see
// MaxForecastHourForCycle() below for the real, live-verified rule.
//
// GetPrefix()/GetTimePointByKey() exist to satisfy AwsNexradDataProvider's
// interface (and keep this class usable through the generic
// ListObjects()/FindKey() base-class API), but -- same as RrfsDataProvider
// -- GribManager's own real fetch path bypasses day-based listing
// entirely, resolving each key deterministically via BuildKey(cycle,
// hour) instead: nowhere near a whole day's objects to list, and the
// exact key is always computable without asking S3 first.
class NbmDataProvider : public AwsNexradDataProvider
{
public:
   NbmDataProvider();
   explicit NbmDataProvider(const std::string& bucketName,
                            const std::string& region);
   ~NbmDataProvider() override;

   NbmDataProvider(const NbmDataProvider&)            = delete;
   NbmDataProvider& operator=(const NbmDataProvider&) = delete;
   NbmDataProvider(NbmDataProvider&&)                 = delete;
   NbmDataProvider& operator=(NbmDataProvider&&)      = delete;

   // The real, live-verified (2026-09-26) forecast-hour availability
   // rule: the 6-hourly "extended" cycles (00/06/12/18z) publish hourly
   // through F069, 3-hourly F072-F189, 6-hourly F192-F264. Every other
   // hourly cycle is reliable only through F036 -- spot checks found
   // some (but not all) short cycles have a few scattered hours beyond
   // that, but real gaps too (confirmed live), so F036 is the
   // conservative cutoff that's always actually there.
   static bool UsesExtendedRange(std::chrono::system_clock::time_point cycleTime);
   static int
   MaxForecastHourForCycle(std::chrono::system_clock::time_point cycleTime);

   // The real, non-uniform step within an extended cycle's own range --
   // hourly through F069, then 3-hourly, then 6-hourly from F192. Short
   // cycles are hourly throughout their (shorter) range. Returns the
   // smallest valid forecast hour >= `hour` for `cycleTime`, clamped to
   // [0, MaxForecastHourForCycle(cycleTime)] -- callers (a slider, a
   // prefetch loop) always get a real, fetchable hour back, never one
   // that falls in a step gap.
   static int SnapForecastHour(std::chrono::system_clock::time_point cycleTime,
                               int                                    hour);

   // Deterministic S3 key for `cycle`'s `hour`-hour CONUS forecast -- no
   // network call, no provider-instance-state mutation (same shape and
   // reason as RrfsDataProvider::BuildKey()).
   static std::string BuildKey(std::chrono::system_clock::time_point cycle,
                               int                                    hour);

   void SetCycle(std::chrono::system_clock::time_point cycleTime);
   void UseLatestCycle();
   [[nodiscard]] bool                                   IsUsingLatestCycle() const;
   [[nodiscard]] std::chrono::system_clock::time_point CurrentCycle() const;

   void              SetForecastHour(int hour);
   [[nodiscard]] int ForecastHour() const;

   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   // Not applicable: NBM files are GRIB2, not a WSR-88D binary format.
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;

   // Public forward to the protected base primitive -- same reason every
   // other provider (Mrms/Rtma/Rrfs) re-exposes its own Download*()
   // method publicly: GribManager calls this on a `provider_` it only
   // holds as a `shared_ptr<AwsNexradDataProvider>`, via a static_cast to
   // this concrete type.
   std::optional<std::string>
   FetchField(const std::string&              key,
             const std::string&               parameter,
             const std::string&               level,
             const std::string&               qualifier,
             const std::string&               outputPath,
             const DownloadProgressCallback& progressCallback = nullptr);

protected:
   std::string GetPrefix(std::chrono::system_clock::time_point date) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
