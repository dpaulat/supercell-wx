#include <scwx/provider/aws_level2_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#if !(defined(_MSC_VER) || defined(__clange__))
#   include <date/date.h>
#endif

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::aws_level2_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::string kDefaultBucketName_ = "noaa-nexrad-level2";
static const std::string kDefaultRegion_     = "us-east-1";

class AwsLevel2DataProvider::Impl
{
public:
   explicit Impl(const std::string& radarSite) : radarSite_ {radarSite} {}

   ~Impl() {}

   std::string radarSite_;
};

AwsLevel2DataProvider::AwsLevel2DataProvider(const std::string& radarSite) :
    AwsLevel2DataProvider(radarSite, kDefaultBucketName_, kDefaultRegion_)
{
}
AwsLevel2DataProvider::AwsLevel2DataProvider(const std::string& radarSite,
                                             const std::string& bucketName,
                                             const std::string& region) :
    AwsNexradDataProvider(radarSite, bucketName, region),
    p(std::make_unique<Impl>(radarSite))
{
}
AwsLevel2DataProvider::~AwsLevel2DataProvider() = default;

AwsLevel2DataProvider::AwsLevel2DataProvider(AwsLevel2DataProvider&&) noexcept =
   default;
AwsLevel2DataProvider&
AwsLevel2DataProvider::operator=(AwsLevel2DataProvider&&) noexcept = default;

std::string
AwsLevel2DataProvider::GetPrefix(std::chrono::system_clock::time_point date)
{
   if (date < std::chrono::system_clock::time_point {})
   {
      date = std::chrono::system_clock::time_point {};
   }

   return fmt::format("{0:%Y/%m/%d}/{1}/", fmt::gmtime(date), p->radarSite_);
}

std::chrono::system_clock::time_point
AwsLevel2DataProvider::GetTimePointByKey(const std::string& key) const
{
   return GetTimePointFromKey(key);
}

std::chrono::system_clock::time_point
AwsLevel2DataProvider::GetTimePointFromKey(const std::string& key)
{
   std::chrono::system_clock::time_point time {};

   const size_t lastSeparator = key.rfind('/');
   const size_t offset =
      (lastSeparator == std::string::npos) ? 0 : lastSeparator + 5;

   // Filename format is GGGGYYYYMMDD_TTTTTT(_V##).gz
   static const size_t formatSize = std::string("YYYYMMDD_TTTTTT").size();

   if (key.size() >= offset + formatSize)
   {
      using namespace std::chrono;

#if !(defined(_MSC_VER) || defined(__clang__))
      using namespace date;
#endif

      static const std::string timeFormat {"%Y%m%d_%H%M%S"};

      std::string        timeStr {key.substr(offset, formatSize)};
      std::istringstream in {timeStr};
      in >> parse(timeFormat, time);

      if (in.fail())
      {
         logger_->warn("Invalid time: \"{}\"", timeStr);
      }
   }
   else
   {
      logger_->warn("Time not parsable from key: \"{}\"", key);
   }

   return time;
}

} // namespace provider
} // namespace scwx
