#pragma once

#include <scwx/provider/aws_nexrad_data_provider.hpp>

#include <optional>
#include <string>

namespace scwx::provider
{

// Third GRIB2 source, alongside MRMS and RTMA. Unlike either of those,
// RRFS publishes many objects per day -- roughly 24 cycles x up to 85
// forecast hours x 2 file types (2dfld/prslev) x multiple domains --
// nowhere near AwsNexradDataProvider::ListObjects's single, unpaginated
// ListObjectsV2 call (S3's 1000-key page cap) if GetPrefix() returned a
// whole-day prefix the way RtmaDataProvider's does. So GetPrefix() here
// is scoped all the way down to one specific cycle-hour's file (2
// objects: the .grib2 and its .idx) -- forecast hour, cycle, and file
// family are all selectable (see SetForecastHour()/SetCycle()/
// SetFileFamily() below), but never more than one file at a time.
//
// Real RRFS S3 layout, confirmed live (2026-09-25/26) against every hour
// of a real day, not assumed from the 3-hourly-only picture this class
// started with:
// - 2dfld: cycles where hour%6==0 (00/06/12/18z) publish the *plain*
//   (non-subh) 3km CONUS file out to F084; cycles where hour%3==0 but
//   hour%6!=0 (03/09/15/21z) publish the same plain filename, but only
//   out to F018; every other hour (01/02/04/05/... -- RRFS really does
//   cycle 24x/day) publishes a *subh*-named variant of the same file
//   (`.subh.` inserted before the forecast-hour segment), also capped at
//   F018. See MaxForecastHourForCycle()/UsesSubhVariant() for this rule
//   as code.
// - prslev: same MaxForecastHourForCycle() cap per cycle hour, but a
//   genuinely different availability rule, not just a naming variant --
//   confirmed live it publishes *only* at the 3-hourly cycles (00/03/
//   06/.../21z, always plain, never `.subh.`), not every hour like
//   2dfld's subh variant covers; other hours have no prslev file at all
//   (neither plain nor subh), a real gap rather than something to guess
//   a filename for. Not a practical problem for the *default* auto-
//   resolve mode (CurrentCycle()'s own lag-adjusted guess already only
//   ever lands on a 3-hourly cycle -- see below), only for an explicit
//   SetCycle() onto an hourly (non-3-hourly) cycle, which just resolves
//   to no key found -- the same graceful miss GribManager's own archive-
//   fetch path already handles for any other out-of-range selection, no
//   special-casing needed here.
//
// GetPrefix(date) is called by the base class with only day-granularity
// (always a midnight-floored day, see AwsNexradDataProvider::Refresh's
// "yesterday"/"today" pair) -- the target cycle's *hour* has to come from
// somewhere else. Two modes:
// - Default (no SetCycle() call): auto-resolves to the latest *3-hourly,
//   non-subh* cycle likely to already be complete, applying a live-
//   measured availability lag (a 12z cycle's F000 file was seen
//   published at ~13:51z, a 00z cycle's at ~01:48z, both ~1h48-1h51m
//   after nominal cycle time) -- unchanged from this class's original
//   behavior, so every existing product built against it keeps working
//   exactly as before.
// - After SetCycle(): uses that exact cycle instead, hourly granularity,
//   no lag guessing (the caller -- a run picker -- presumably already
//   knows this cycle is real, e.g. from a directory listing).
// If the resolved cycle's own calendar day doesn't match the `date`
// argument (the near-UTC-midnight case Refresh's yesterday/today pairing
// exists for), GetPrefix returns a deliberately-nonexistent prefix for
// that call instead of a wrong one -- the other call in the pair will be
// the one whose day actually matches.
// RRFS's two per-cycle file types this class can target -- see the class
// comment above for their real, differing hourly-availability rules.
// TwoDField (default) is every existing product's behavior, unchanged.
enum class RrfsFileFamily
{
   TwoDField,
   PressureLevel
};

class RrfsDataProvider : public AwsNexradDataProvider
{
public:
   explicit RrfsDataProvider();
   explicit RrfsDataProvider(const std::string& bucketName,
                             const std::string& region);
   ~RrfsDataProvider() override;

   RrfsDataProvider(const RrfsDataProvider&)            = delete;
   RrfsDataProvider& operator=(const RrfsDataProvider&) = delete;
   RrfsDataProvider(RrfsDataProvider&&)                 = delete;
   RrfsDataProvider& operator=(RrfsDataProvider&&)      = delete;

   // 84 for a 6-hourly cycle (00/06/12/18z), 18 for anything else --
   // pure function of the cycle's own hour, no live data needed, so a
   // run/hour-picker UI can size itself before ever fetching anything.
   [[nodiscard]] static int
   MaxForecastHourForCycle(std::chrono::system_clock::time_point cycleTime);

   // Whether this cycle's 2dfld file needs ".subh." in its name -- true
   // for every hour except the 3-hourly ones (00/03/06/.../21z), which
   // use the plain filename regardless of whether they're a 84h or 18h
   // run.
   [[nodiscard]] static bool
   UsesSubhVariant(std::chrono::system_clock::time_point cycleTime);

   // The exact S3 object key for `cycle`'s `family` file at forecast hour
   // `hour` (clamped to [0, MaxForecastHourForCycle(cycle)]) -- computed
   // directly from the same naming rule GetPrefix() uses, with no
   // ListObjectsV2 call and no dependency on (or mutation of) any provider
   // instance's own SetCycle()/SetForecastHour()/SetFileFamily() state.
   // Exists so a caller that wants many hours' worth of keys at once (a
   // prefetch pass -- see GribManager::PrefetchRrfsForecastHourRange())
   // can compute every one up front without racing the one live provider
   // instance actually driving the current on-screen selection. For
   // PressureLevel, always builds the plain (never `.subh.`) filename
   // regardless of `cycle`'s own hour -- prslev simply has no subh
   // variant (see class comment); an hourly (non-3-hourly) cycle just
   // yields a key that doesn't exist on S3, the same graceful miss any
   // other out-of-range selection already produces.
   [[nodiscard]] static std::string
   BuildKey(std::chrono::system_clock::time_point cycle,
            int                                   hour,
            RrfsFileFamily family = RrfsFileFamily::TwoDField);

   // Overrides auto-resolution with an exact cycle (floored to the
   // hour) -- see class comment. Does not itself trigger a fetch;
   // the next Refresh()/FindLatestKey() picks it up.
   void SetCycle(std::chrono::system_clock::time_point cycleTime);

   // Reverts to the original auto-resolved "latest complete 3-hourly
   // cycle" behavior.
   void UseLatestCycle();

   [[nodiscard]] bool IsUsingLatestCycle() const;

   // The cycle GetPrefix() is currently targeting -- either the
   // SetCycle() override, or (if auto-resolving) the same lag-adjusted
   // guess GetPrefix() itself would compute, exposed so a run picker can
   // show what "latest" actually resolved to.
   [[nodiscard]] std::chrono::system_clock::time_point CurrentCycle() const;

   // 0 (default) selects F000. Clamped to
   // [0, MaxForecastHourForCycle(CurrentCycle())] the next time GetPrefix()
   // runs, not here -- CurrentCycle() can still change afterward (e.g. a
   // fresh auto-resolve) in a way that changes the valid range.
   void              SetForecastHour(int hour);
   [[nodiscard]] int ForecastHour() const;

   // TwoDField (default) unless set otherwise -- a fixed, per-instance
   // choice in practice (GribManager constructs one RrfsDataProvider per
   // active product and sets this once, right after construction, never
   // toggling it afterward on a live instance -- see MakeProvider()),
   // not something a cycle/hour picker UI flips at runtime the way
   // SetCycle()/SetForecastHour() are.
   void                         SetFileFamily(RrfsFileFamily family);
   [[nodiscard]] RrfsFileFamily FileFamily() const;

   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;

   // Downloads the S3 object for `key` as-is (no decompression, same as
   // RtmaDataProvider -- confirmed live that RRFS's S3 objects are plain
   // GRIB2) and writes it to `outputPath`, optionally reporting
   // incremental progress via `progressCallback` (see
   // AwsNexradDataProvider::DownloadProgressCallback's own doc -- a real
   // one to wire up here: a 2dfld file is ~320MB, easily the single
   // longest-running step of any RRFS fetch). Returns outputPath on
   // success, std::nullopt on failure.
   std::optional<std::string>
   DownloadRaw(const std::string&              key,
               const std::string&              outputPath,
               const DownloadProgressCallback& progressCallback = nullptr);

protected:
   std::string GetPrefix(std::chrono::system_clock::time_point date) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
