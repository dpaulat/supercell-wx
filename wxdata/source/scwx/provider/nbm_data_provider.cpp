#include <scwx/provider/nbm_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <regex>

#include <fmt/chrono.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::nbm_data_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Real, live-measured (2026-09-26) publish lag for a cycle's F001 core
// file: 36-50 minutes past the nominal cycle hour across 4 samples. 1
// hour is a conservative round margin above that -- same reasoning as
// RrfsDataProvider's own kAvailabilityLag_: a wrong guess just costs one
// empty poll via GribManager's periodic Refresh(), not a hard failure.
static constexpr std::chrono::hours kAvailabilityLag_ {1};

// The real, live-verified (2026-09-26) step boundaries within an
// extended (00/06/12/18z) cycle's own forecast range: hourly through
// this hour, then 3-hourly, then 6-hourly from k3HourlyEnd_+3 (192).
static constexpr int kHourlyEnd_  = 69;
static constexpr int k3HourlyEnd_ = 189;

// Confirmed live (2026-09-26): unlike RRFS/GFS, NBM does not publish an
// F000 file at all for the CONUS core product ("co") -- F001 is the
// first real forecast hour for every cycle. A first version of this
// class defaulted to/allowed F000, which 404'd on every single fetch
// until a caller explicitly picked a real hour.
static constexpr int kMinForecastHour_ = 1;

class NbmDataProvider::Impl
{
public:
   explicit Impl(std::string bucketName) : bucketName_ {std::move(bucketName)}
   {
   }
   ~Impl() = default;

   // Same reason as every other AwsNexradDataProvider subclass's own
   // bucketName_: the base class stores its copy privately with no
   // accessor.
   std::string bucketName_;

   bool                                  useLatestCycle_ {true};
   std::chrono::system_clock::time_point cycleOverride_ {};
   int                                   forecastHour_ {kMinForecastHour_};
};

NbmDataProvider::NbmDataProvider() : NbmDataProvider("noaa-nbm-grib2-pds", "us-east-1")
{
}

NbmDataProvider::NbmDataProvider(const std::string& bucketName,
                                 const std::string& region) :
    // "nbm" fills AwsNexradDataProvider's radarSite slot -- unused by
    // this class's own GetPrefix()/GetTimePointByKey(), just a label.
    AwsNexradDataProvider("nbm", bucketName, region),
    p(std::make_unique<Impl>(bucketName))
{
}

NbmDataProvider::~NbmDataProvider() = default;

bool NbmDataProvider::UsesExtendedRange(
   std::chrono::system_clock::time_point cycleTime)
{
   using namespace std::chrono;
   const auto day  = floor<days>(cycleTime);
   const auto hour = duration_cast<hours>(cycleTime - day).count();
   return hour % 6 == 0;
}

int NbmDataProvider::MaxForecastHourForCycle(
   std::chrono::system_clock::time_point cycleTime)
{
   return UsesExtendedRange(cycleTime) ? 264 : 36;
}

int NbmDataProvider::SnapForecastHour(
   std::chrono::system_clock::time_point cycleTime, int hour)
{
   const int maxHour = MaxForecastHourForCycle(cycleTime);
   hour               = std::clamp(hour, kMinForecastHour_, maxHour);

   if (!UsesExtendedRange(cycleTime) || hour <= kHourlyEnd_)
   {
      return hour;
   }

   if (hour <= k3HourlyEnd_)
   {
      constexpr int base = kHourlyEnd_ + 3; // 72
      constexpr int step = 3;
      return base + step * ((hour - base + step - 1) / step);
   }

   constexpr int base6 = k3HourlyEnd_ + 3; // 192
   constexpr int step6 = 6;
   return std::min(maxHour,
                   base6 + step6 * ((hour - base6 + step6 - 1) / step6));
}

void NbmDataProvider::SetCycle(std::chrono::system_clock::time_point cycleTime)
{
   p->cycleOverride_  = std::chrono::floor<std::chrono::hours>(cycleTime);
   p->useLatestCycle_ = false;
}

void NbmDataProvider::UseLatestCycle()
{
   p->useLatestCycle_ = true;
}

bool NbmDataProvider::IsUsingLatestCycle() const
{
   return p->useLatestCycle_;
}

std::chrono::system_clock::time_point NbmDataProvider::CurrentCycle() const
{
   using namespace std::chrono;

   if (!p->useLatestCycle_)
   {
      return p->cycleOverride_;
   }

   return floor<hours>(util::time::now() - kAvailabilityLag_);
}

void NbmDataProvider::SetForecastHour(int hour)
{
   p->forecastHour_ = hour;
}

int NbmDataProvider::ForecastHour() const
{
   return p->forecastHour_;
}

std::string
NbmDataProvider::GetPrefix(std::chrono::system_clock::time_point date)
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
      // Same deliberately-nonexistent-prefix trick RrfsDataProvider's own
      // GetPrefix() uses -- "99" is not a valid UTC hour, so this call
      // just lists 0 objects rather than the wrong day's.
      return fmt::format("blend.{0:%Y%m%d}/99/", fmt::gmtime(date));
   }

   const std::string key = BuildKey(cycle, p->forecastHour_);
   static const std::string kGribSuffix {".grib2"};
   return key.substr(0, key.size() - kGribSuffix.size());
}

std::string NbmDataProvider::BuildKey(std::chrono::system_clock::time_point cycle,
                                      int                                    hour)
{
   using namespace std::chrono;

   const auto cycleDay  = floor<days>(cycle);
   const auto cycleHour = duration_cast<hours>(cycle - cycleDay).count();
   const int  maxHour   = MaxForecastHourForCycle(cycle);
   hour                 = std::clamp(hour, kMinForecastHour_, maxHour);

   // NBM's S3 layout: blend.<YYYYMMDD>/<HH>/core/blend.t<HH>z.core.
   // f<FFF>.co.grib2 -- "co" = CONUS, the only domain this class targets
   // (ak/hi/gu/pr also exist but aren't relevant to a CONUS radar
   // viewer).
   return fmt::format(
      "blend.{0:%Y%m%d}/{1:02d}/core/blend.t{1:02d}z.core.f{2:03d}.co.grib2",
      fmt::gmtime(cycleDay),
      cycleHour,
      hour);
}

std::chrono::system_clock::time_point
NbmDataProvider::GetTimePointByKey(const std::string& key) const
{
   std::chrono::system_clock::time_point time {};

   // The trailing $ is load-bearing -- see RtmaDataProvider's own comment
   // on the same anchor: without it, this file's ".idx" sidecar also
   // matches via regex_search finding the pattern as a substring.
   static const std::regex kTimeRegex {
      R"(blend\.(\d{8})/(\d{2})/core/blend\.t\d{2}z\.core\.f(\d{3})\.co\.grib2$)"};
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
      logger_->debug("Key is not an NBM CONUS core file: \"{}\"", key);
   }

   return time;
}

std::shared_ptr<wsr88d::NexradFile>
NbmDataProvider::LoadObjectByKey(const std::string& /* key */)
{
   logger_->warn(
      "LoadObjectByKey() is not applicable to NBM data, use FetchField() "
      "instead");
   return nullptr;
}

std::shared_ptr<wsr88d::NexradFile> NbmDataProvider::LoadObjectByTime(
   std::chrono::system_clock::time_point /* time */)
{
   logger_->warn(
      "LoadObjectByTime() is not applicable to NBM data, use FetchField() "
      "instead");
   return nullptr;
}

std::optional<std::string>
NbmDataProvider::FetchField(const std::string&              key,
                            const std::string&              parameter,
                            const std::string&               level,
                            const std::string&               qualifier,
                            const std::string&               outputPath,
                            const DownloadProgressCallback& progressCallback)
{
   return DownloadGribMessageByIndex(
      p->bucketName_, key, parameter, level, qualifier, outputPath, progressCallback);
}

} // namespace scwx::provider
