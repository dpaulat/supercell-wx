#include <scwx/provider/aws_level3_data_provider.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <shared_mutex>

#include <aws/s3/model/ListObjectsV2Request.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#if !defined(_MSC_VER)
#   include <date/date.h>
#endif

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::aws_level3_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::string kDefaultBucketName_ = "unidata-nexrad-level3";
static const std::string kDefaultRegion_     = "us-east-1";

static std::unordered_map<std::string, std::vector<std::string>> productMap_;
static std::shared_mutex                                         productMutex_;

class AwsLevel3DataProvider::Impl
{
public:
   explicit Impl(AwsLevel3DataProvider* self,
                 const std::string&     radarSite,
                 const std::string&     product,
                 const std::string&     bucketName) :
       self_ {self},
       radarSite_ {radarSite},
       siteId_ {common::GetSiteId(radarSite_)},
       product_ {product},
       bucketName_ {bucketName}
   {
   }
   ~Impl() = default;

   void ListProducts();

   AwsLevel3DataProvider* self_;

   std::string radarSite_;
   std::string siteId_;
   std::string product_;
   std::string bucketName_;
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
    p(std::make_unique<Impl>(this, radarSite, product, bucketName))
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

#if !defined(_MSC_VER)
      using namespace date;
#endif

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

void AwsLevel3DataProvider::RequestAvailableProducts()
{
   p->ListProducts();
}

std::vector<std::string> AwsLevel3DataProvider::GetAvailableProducts()
{
   std::shared_lock readLock(productMutex_);

   auto siteProductMap = productMap_.find(p->radarSite_);
   if (siteProductMap != productMap_.cend())
   {
      return siteProductMap->second;
   }

   return {};
}

void AwsLevel3DataProvider::Impl::ListProducts()
{
   std::shared_lock readLock(productMutex_);

   // Only list once per radar site
   if (productMap_.contains(radarSite_))
   {
      return;
   }

   readLock.unlock();

   logger_->debug("ListProducts()");

   // Prefix format: GGG_
   const std::string prefix = fmt::format("{0}_", siteId_);

   Aws::S3::Model::ListObjectsV2Request request;
   request.SetBucket(bucketName_);
   request.SetPrefix(prefix);
   request.SetDelimiter("_");

   auto outcome = self_->client()->ListObjectsV2(request);

   if (outcome.IsSuccess())
   {
      std::unique_lock writeLock(productMutex_);

      // If the product was populated since first checked, don't re-populate
      if (productMap_.contains(radarSite_))
      {
         return;
      }

      auto& prefixes = outcome.GetResult().GetCommonPrefixes();

      // Create a vector with reserved capacity
      std::vector<std::string> productList;
      productList.reserve(prefixes.size());

      std::for_each(prefixes.cbegin(),
                    prefixes.cend(),
                    [&](const Aws::S3::Model::CommonPrefix& commonPrefix)
                    {
                       // Prefix format: GGG_PPP_
                       std::string prefix = commonPrefix.GetPrefix();
                       size_t      left   = prefix.find('_');
                       size_t      right  = prefix.rfind('_');

                       // If left != npos, right != npos
                       if (left != std::string::npos && right > left)
                       {
                          // The product starts after the left delimeter, and
                          // ends before the right delimeter.
                          ++left;
                          productList.push_back(
                             prefix.substr(left, right - left));
                       }
                    });

      // Remove extra capacity if necessary
      productList.shrink_to_fit();

      productMap_.emplace(radarSite_, std::move(productList));
   }
}

} // namespace provider
} // namespace scwx
