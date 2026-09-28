#include <scwx/provider/rrfs_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <regex>

#include <fmt/chrono.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::rrfs_data_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// See class comment: RRFS cycles are not available the instant their
// nominal hour arrives -- a live check found a 12z cycle's F000 2dfld
// file wasn't published (S3 LastModified) until ~13:51z. 2 hours is a
// starting guess with that one data point behind it, not a documented
// SLA -- if this proves too aggressive or too conservative once this
// runs for a while, adjust it; the periodic Refresh() polling this
// provider gets from GribManager means a wrong guess just costs one
// empty poll, not a hard failure.
static constexpr std::chrono::hours kAvailabilityLag_ {2};

class RrfsDataProvider::Impl
{
public:
   explicit Impl(std::string bucketName) : bucketName_ {std::move(bucketName)}
   {
   }
   ~Impl() = default;

   // Same reason as Mrms/RtmaDataProvider::Impl::bucketName_:
   // AwsNexradDataProvider stores its own copy privately with no accessor.
   std::string bucketName_;

   // See SetCycle()/UseLatestCycle()/SetForecastHour() -- cycleOverride_
   // is only meaningful while useLatestCycle_ is false.
   bool                                  useLatestCycle_ {true};
   std::chrono::system_clock::time_point cycleOverride_ {};
   int                                   forecastHour_ {0};
   RrfsFileFamily fileFamily_ {RrfsFileFamily::TwoDField};
};

RrfsDataProvider::RrfsDataProvider() :
    RrfsDataProvider("noaa-rrfs-ops-pds", "us-east-1")
{
}

RrfsDataProvider::RrfsDataProvider(const std::string& bucketName,
                                   const std::string& region) :
    // "rrfs" fills AwsNexradDataProvider's radarSite slot -- unused by
    // this class's own GetPrefix()/GetTimePointByKey(), just a label.
    AwsNexradDataProvider("rrfs", bucketName, region),
    p(std::make_unique<Impl>(bucketName))
{
}

RrfsDataProvider::~RrfsDataProvider() = default;

int RrfsDataProvider::MaxForecastHourForCycle(
   std::chrono::system_clock::time_point cycleTime)
{
   using namespace std::chrono;
   const auto day  = floor<days>(cycleTime);
   const auto hour = duration_cast<hours>(cycleTime - day).count();
   return (hour % 6 == 0) ? 84 : 18;
}

bool RrfsDataProvider::UsesSubhVariant(
   std::chrono::system_clock::time_point cycleTime)
{
   using namespace std::chrono;
   const auto day  = floor<days>(cycleTime);
   const auto hour = duration_cast<hours>(cycleTime - day).count();
   return (hour % 3 != 0);
}

void RrfsDataProvider::SetCycle(std::chrono::system_clock::time_point cycleTime)
{
   p->cycleOverride_  = std::chrono::floor<std::chrono::hours>(cycleTime);
   p->useLatestCycle_ = false;
}

void RrfsDataProvider::UseLatestCycle()
{ p->useLatestCycle_ = true; }

bool RrfsDataProvider::IsUsingLatestCycle() const
{ return p->useLatestCycle_; }

std::chrono::system_clock::time_point RrfsDataProvider::CurrentCycle() const
{
   using namespace std::chrono;

   if (!p->useLatestCycle_)
   {
      return p->cycleOverride_;
   }

   // Same lag-adjusted "latest complete 3-hourly cycle" guess GetPrefix()
   // always used before SetCycle() existed -- see class comment.
   const auto targetTime = util::time::now() - kAvailabilityLag_;
   const auto targetDay  = floor<days>(targetTime);
   const auto rawHour    = duration_cast<hours>(targetTime - targetDay).count();
   const auto hour       = (rawHour / 3) * 3;

   return targetDay + hours {hour};
}

void RrfsDataProvider::SetForecastHour(int hour)
{ p->forecastHour_ = hour; }

int RrfsDataProvider::ForecastHour() const
{ return p->forecastHour_; }

void RrfsDataProvider::SetFileFamily(RrfsFileFamily family)
{ p->fileFamily_ = family; }

RrfsFileFamily RrfsDataProvider::FileFamily() const
{ return p->fileFamily_; }

std::string
RrfsDataProvider::GetPrefix(std::chrono::system_clock::time_point date)
{
   using namespace std::chrono;

   if (date < system_clock::time_point {})
   {
      date = system_clock::time_point {};
   }

   const auto cycle    = CurrentCycle();
   const auto cycleDay = floor<days>(cycle);

   if (cycleDay != floor<days>(date))
   {
      // `date` (always midnight-floored -- see class comment) isn't the
      // day the resolved cycle actually falls on. Rather than build a
      // prefix for the wrong day (or worse, mismatch date/hour), return
      // one that can't match anything real -- "99" is not a valid UTC
      // hour -- so this call just lists 0 objects instead of the wrong
      // ones. The *other* call in Refresh's yesterday/today pair (or
      // EnsureDateListed's own day-neighbor retries, for an explicit
      // SetCycle()) is the one whose day will actually match.
      return fmt::format("rrfs.{0:%Y%m%d}/99/", fmt::gmtime(date));
   }

   // Scoped all the way to one specific file (see class comment for why:
   // nowhere near a whole day's worth of objects, which would blow past
   // ListObjectsV2's unpaginated 1000-key cap) -- BuildKey() computes the
   // exact key; strip its ".grib2" suffix for use as a ListObjectsV2
   // prefix, which also matches the ".idx" sidecar the same way the full
   // key match once did.
   const std::string key = BuildKey(cycle, p->forecastHour_, p->fileFamily_);
   static const std::string kGribSuffix {".grib2"};
   return key.substr(0, key.size() - kGribSuffix.size());
}

std::string RrfsDataProvider::BuildKey(
   std::chrono::system_clock::time_point cycle, int hour, RrfsFileFamily family)
{
   using namespace std::chrono;

   const auto cycleDay  = floor<days>(cycle);
   const auto cycleHour = duration_cast<hours>(cycle - cycleDay).count();
   const int  maxHour   = MaxForecastHourForCycle(cycle);
   hour                 = std::clamp(hour, 0, maxHour);

   const char* fileType =
      (family == RrfsFileFamily::PressureLevel) ? "prslev" : "2dfld";

   // RRFS's S3 layout: rrfs.<YYYYMMDD>/<HH>/rrfs.t<HH>z.<fileType>.3km.
   // [subh.]fNNN.conus.grib2 -- ".subh." is required for 2dfld at every
   // cycle except the 3-hourly ones (see UsesSubhVariant's own comment);
   // prslev never uses it at all (see class comment: it simply isn't
   // published outside the 3-hourly cycles, not renamed there).
   if (family == RrfsFileFamily::TwoDField && UsesSubhVariant(cycle))
   {
      return fmt::format(
         "rrfs.{0:%Y%m%d}/{1:02d}/rrfs.t{1:02d}z.{3}.3km."
         "subh.f{2:03d}.conus.grib2",
         fmt::gmtime(cycleDay),
         cycleHour,
         hour,
         fileType);
   }
   else
   {
      return fmt::format(
         "rrfs.{0:%Y%m%d}/{1:02d}/rrfs.t{1:02d}z.{3}.3km.f{2:03d}.conus."
         "grib2",
         fmt::gmtime(cycleDay),
         cycleHour,
         hour,
         fileType);
   }
}

std::chrono::system_clock::time_point
RrfsDataProvider::GetTimePointByKey(const std::string& key) const
{
   std::chrono::system_clock::time_point time {};

   // GetPrefix() already scopes ListObjectsV2 to exactly this file (plus
   // its .idx sidecar), so this regex exists to reject the sidecar (and
   // anything else unexpected) rather than to discriminate between many
   // real candidates the way RTMA's own version has to. Matches both
   // 2dfld and prslev, and both the plain and ".subh." filename variants
   // (see class comment) -- prslev never actually publishes a `.subh.`
   // key (see class comment), but matching it here anyway costs nothing
   // and keeps this regex a strict superset of BuildKey()'s own output
   // rather than a second place that has to independently track which
   // combinations are real. Captures the forecast hour -- the returned
   // time is the file's own *valid* time (cycle + forecast hour), not
   // just the cycle's nominal time, so different forecast-hour
   // selections of the same cycle don't collide on the same TimePoint in
   // AwsNexradDataProvider's own internal objects_ map.
   static const std::regex kTimeRegex {
      R"(rrfs\.(\d{8})/(\d{2})/rrfs\.t\d{2}z\.(?:2dfld|prslev)\.3km\.(?:subh\.)?f(\d{3})\.conus\.grib2$)"};
   std::smatch match;

   if (std::regex_search(key, match, kTimeRegex))
   {
      const std::string dateStr =
         match[1].str() + " " + match[2].str() + "0000";
      const int forecastHour = std::stoi(match[3].str());

      using namespace std::chrono;
#if (__cpp_lib_chrono < 201907L)
      using namespace date;
#endif

      std::istringstream                    in {dateStr};
      std::chrono::system_clock::time_point cycleTime;
      in >> parse("%Y%m%d %H%M%S", cycleTime);

      if (in.fail())
      {
         logger_->warn("Invalid time: \"{}\"", dateStr);
      }
      else
      {
         time = cycleTime + std::chrono::hours {forecastHour};
      }
   }
   else
   {
      logger_->debug("Key is not a 2dfld/prslev CONUS file: \"{}\"", key);
   }

   return time;
}

std::shared_ptr<wsr88d::NexradFile>
RrfsDataProvider::LoadObjectByKey(const std::string& /* key */)
{
   // Not applicable: RRFS files are GRIB2, not a WSR-88D binary format.
   // Use DownloadRaw() instead.
   logger_->warn(
      "LoadObjectByKey() is not applicable to RRFS data, use DownloadRaw() "
      "instead");
   return nullptr;
}

std::shared_ptr<wsr88d::NexradFile> RrfsDataProvider::LoadObjectByTime(
   std::chrono::system_clock::time_point /* time */)
{
   logger_->warn(
      "LoadObjectByTime() is not applicable to RRFS data, use DownloadRaw() "
      "instead");
   return nullptr;
}

std::optional<std::string>
RrfsDataProvider::DownloadRaw(const std::string&              key,
                              const std::string&              outputPath,
                              const DownloadProgressCallback& progressCallback)
{ return DownloadObject(p->bucketName_, key, outputPath, progressCallback); }

} // namespace scwx::provider
