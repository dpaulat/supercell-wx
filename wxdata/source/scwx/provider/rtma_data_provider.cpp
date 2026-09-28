#include <scwx/provider/rtma_data_provider.hpp>
#include <scwx/util/logger.hpp>

#include <regex>

#include <fmt/chrono.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::rtma_data_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RtmaDataProvider::Impl
{
public:
   explicit Impl(std::string bucketName) : bucketName_ {std::move(bucketName)}
   {
   }
   ~Impl() = default;

   // Same reason as MrmsDataProvider::Impl::bucketName_: AwsNexradDataProvider
   // stores its own copy privately with no accessor.
   std::string bucketName_;
};

RtmaDataProvider::RtmaDataProvider() :
    RtmaDataProvider("noaa-rtma-pds", "us-east-1")
{
}

RtmaDataProvider::RtmaDataProvider(const std::string& bucketName,
                                   const std::string& region) :
    // "rtma2p5" fills AwsNexradDataProvider's radarSite slot -- unused by
    // this class's own GetPrefix()/GetTimePointByKey() (there is no
    // per-product path to build, see class comment), just a label.
    AwsNexradDataProvider("rtma2p5", bucketName, region),
    p(std::make_unique<Impl>(bucketName))
{
}

RtmaDataProvider::~RtmaDataProvider() = default;

std::string
RtmaDataProvider::GetPrefix(std::chrono::system_clock::time_point date)
{
   if (date < std::chrono::system_clock::time_point {})
   {
      date = std::chrono::system_clock::time_point {};
   }

   // RTMA's S3 layout:
   // rtma2p5.<YYYYMMDD>/rtma2p5.t<HH>z.2dvaranl_ndfd.grb2_wexp
   return fmt::format("rtma2p5.{0:%Y%m%d}/", fmt::gmtime(date));
}

std::chrono::system_clock::time_point
RtmaDataProvider::GetTimePointByKey(const std::string& key) const
{
   std::chrono::system_clock::time_point time {};

   // Each date folder also holds .idx/.gif siblings and other RTMA
   // products (rqirtma, pcprtma, 2dvarerr, 2dvarges) -- only the
   // 2dvaranl_ndfd analysis file matches this pattern, which is expected
   // and not a parse failure, so a miss logs at debug rather than warn
   // (contrast MrmsDataProvider, where every listed key is expected to
   // match). The trailing $ is load-bearing, not decorative: without it,
   // the file's own ".idx" sidecar (rtma2p5....grb2_wexp.idx) also
   // matches via regex_search finding the pattern as a substring, and
   // since objects_ is keyed by parsed *time* (not by key string), the
   // .idx entry silently clobbers the real data file's entry for the same
   // hour -- confirmed as a real, live bug (FindLatestKey() returning an
   // .idx path) before this anchor was added.
   static const std::regex kTimeRegex {
      R"(rtma2p5\.(\d{8})/rtma2p5\.t(\d{2})z\.2dvaranl_ndfd\.grb2_wexp$)"};
   std::smatch match;

   if (std::regex_search(key, match, kTimeRegex))
   {
      // "%H%M%S" needs 6 digits (HHMMSS); RTMA's filename only carries the
      // hour, so pad both minutes and seconds -- always "00" for an
      // on-the-hour analysis.
      const std::string dateStr =
         match[1].str() + " " + match[2].str() + "0000";

      using namespace std::chrono;
#if (__cpp_lib_chrono < 201907L)
      using namespace date;
#endif

      std::istringstream in {dateStr};
      in >> parse("%Y%m%d %H%M%S", time);

      if (in.fail())
      {
         logger_->warn("Invalid time: \"{}\"", dateStr);
      }
   }
   else
   {
      logger_->debug("Key is not an hourly RTMA analysis file: \"{}\"", key);
   }

   return time;
}

std::shared_ptr<wsr88d::NexradFile>
RtmaDataProvider::LoadObjectByKey(const std::string& /* key */)
{
   // Not applicable: RTMA files are GRIB2, not a WSR-88D binary format.
   // Use DownloadRaw() instead.
   logger_->warn(
      "LoadObjectByKey() is not applicable to RTMA data, use DownloadRaw() "
      "instead");
   return nullptr;
}

std::shared_ptr<wsr88d::NexradFile> RtmaDataProvider::LoadObjectByTime(
   std::chrono::system_clock::time_point /* time */)
{
   logger_->warn(
      "LoadObjectByTime() is not applicable to RTMA data, use DownloadRaw() "
      "instead");
   return nullptr;
}

std::optional<std::string>
RtmaDataProvider::DownloadRaw(const std::string&              key,
                              const std::string&              outputPath,
                              const DownloadProgressCallback& progressCallback)
{ return DownloadObject(p->bucketName_, key, outputPath, progressCallback); }

} // namespace scwx::provider
