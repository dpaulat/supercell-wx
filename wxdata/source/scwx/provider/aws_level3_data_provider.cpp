#include <scwx/provider/aws_level3_data_provider.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::aws_level3_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::string kDefaultBucketName_ = "unidata-nexrad-level3";
static const std::string kDefaultRegion_     = "us-east-1";

class AwsLevel3DataProvider::Impl
{
public:
   explicit Impl(const std::string& radarSite, const std::string& product) :
       radarSite_ {radarSite},
       siteId_ {common::GetSiteId(radarSite_)},
       product_ {product}
   {
   }

   ~Impl() {}

   std::string radarSite_;
   std::string siteId_;
   std::string product_;
};

AwsLevel3DataProvider::AwsLevel3DataProvider(const std::string& radarSite,
                                             const std::string& product) :
    AwsLevel3DataProvider(
       radarSite, product, kDefaultBucketName_, kDefaultRegion_)
{
}
AwsLevel3DataProvider::AwsLevel3DataProvider(const std::string& radarSite,
                                             const std::string& product,
                                             const std::string& bucketName,
                                             const std::string& region) :
    AwsNexradDataProvider(radarSite, bucketName, region),
    p(std::make_unique<Impl>(radarSite, product))
{
}
AwsLevel3DataProvider::~AwsLevel3DataProvider() = default;

AwsLevel3DataProvider::AwsLevel3DataProvider(AwsLevel3DataProvider&&) noexcept =
   default;
AwsLevel3DataProvider&
AwsLevel3DataProvider::operator=(AwsLevel3DataProvider&&) noexcept = default;

std::string
AwsLevel3DataProvider::GetPrefix(std::chrono::system_clock::time_point date)
{
   return fmt::format(
      "{0}_{1}_{2:%Y_%m_%d}_", p->siteId_, p->product_, fmt::gmtime(date));
}

std::chrono::system_clock::time_point
AwsLevel3DataProvider::GetTimePointByKey(const std::string& key) const
{
   return GetTimePointFromKey(key);
}

std::chrono::system_clock::time_point
AwsLevel3DataProvider::GetTimePointFromKey(const std::string& key)
{
   std::chrono::system_clock::time_point time {};

   constexpr size_t offset = 8;

   // Filename format is GGG_PPP_YYYY_MM_DD_HH_MM_SS
   static constexpr size_t formatSize =
      std::string("YYYY_MM_DD_HH_MM_SS").size();

   if (key.size() >= offset + formatSize)
   {
      using namespace std::chrono;

      static const std::string timeFormat {"%Y_%m_%d_%H_%M_%S"};

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
