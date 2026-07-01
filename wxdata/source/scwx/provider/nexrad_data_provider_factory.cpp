#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/provider/aws_level2_data_provider.hpp>
#include <scwx/provider/aws_level2_chunks_data_provider.hpp>
#include <scwx/provider/aws_level3_data_provider.hpp>
#include <scwx/provider/http_level3_data_provider.hpp>
#include <scwx/provider/ondas_level2_data_provider.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <boost/algorithm/string/predicate.hpp>

namespace scwx::provider
{

static const std::string logPrefix_ =
   "scwx::provider::nexrad_data_provider_factory";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::string kS3Prefix_    = "s3://";
static const std::string kHttpPrefix_  = "http://";
static const std::string kHttpsPrefix_ = "https://";

static const std::string kDefaultS3Region_ = "us-east-1";

static std::string ExtractBucketNameFromS3Uri(const std::string& s3Uri);

std::shared_ptr<NexradDataProvider>
NexradDataProviderFactory::CreateLevel2DataProvider(
   const std::string& radarSite)
{
   const std::string level2Url =
      util::GetEnvironment("SCWX_LEVEL2_DATA_PROVIDER_URL");

   if (level2Url.empty())
   {
      return std::make_shared<AwsLevel2DataProvider>(radarSite);
   }
   else
   {
      return CreateLevel2DataProvider(radarSite, level2Url);
   }
}

std::shared_ptr<NexradDataProvider>
NexradDataProviderFactory::CreateLevel2DataProvider(
   const std::string& radarSite, const std::string& baseUri)
{
   std::shared_ptr<NexradDataProvider> provider = nullptr;

   if (boost::istarts_with(baseUri, kS3Prefix_))
   {
      // Extract bucket and region from baseUri
      // Expected format: s3://bucket-name/
      const std::string  bucketName = ExtractBucketNameFromS3Uri(baseUri);
      const std::string& region     = kDefaultS3Region_; // Default region

      provider =
         std::make_shared<AwsLevel2DataProvider>(radarSite, bucketName, region);
   }
   else if (boost::istarts_with(baseUri, kHttpPrefix_) ||
            boost::istarts_with(baseUri, kHttpsPrefix_))
   {
      // ONDAS is the supported Level 2 HTTP-based provider
      provider = std::make_shared<OndasLevel2DataProvider>(radarSite, baseUri);
   }
   else
   {
      // Unrecognized URI scheme
      logger_->warn("Unrecognized URI scheme in baseUri: \"{}\"", baseUri);
   }

   return provider;
}

std::shared_ptr<NexradDataProvider>
NexradDataProviderFactory::CreateLevel2ChunksDataProvider(
   const std::string& radarSite)
{
   return std::make_shared<AwsLevel2ChunksDataProvider>(radarSite);
}

std::shared_ptr<NexradDataProvider>
NexradDataProviderFactory::CreateLevel3DataProvider(
   const std::string& radarSite, const std::string& product)
{
   const std::string level3Url =
      util::GetEnvironment("SCWX_LEVEL3_DATA_PROVIDER_URL");

   if (level3Url.empty())
   {
      return std::make_shared<AwsLevel3DataProvider>(radarSite, product);
   }
   else
   {
      return CreateLevel3DataProvider(radarSite, product, level3Url);
   }
}

std::shared_ptr<NexradDataProvider>
NexradDataProviderFactory::CreateLevel3DataProvider(
   const std::string& radarSite,
   const std::string& product,
   const std::string& baseUri)
{
   std::shared_ptr<NexradDataProvider> provider = nullptr;

   if (boost::istarts_with(baseUri, kS3Prefix_))
   {
      // Extract bucket and region from baseUri
      // Expected format: s3://bucket-name/
      const std::string  bucketName = ExtractBucketNameFromS3Uri(baseUri);
      const std::string& region     = kDefaultS3Region_; // Default region

      provider = std::make_shared<AwsLevel3DataProvider>(
         radarSite, product, bucketName, region);
   }
   else if (boost::istarts_with(baseUri, kHttpPrefix_) ||
            boost::istarts_with(baseUri, kHttpsPrefix_))
   {
      // HTTP-based provider
      provider =
         std::make_shared<HttpLevel3DataProvider>(radarSite, product, baseUri);
   }
   else
   {
      // Unrecognized URI scheme
      logger_->warn("Unrecognized URI scheme in baseUri: \"{}\"", baseUri);
   }

   return provider;
}

static std::string ExtractBucketNameFromS3Uri(const std::string& s3Uri)
{
   if (boost::istarts_with(s3Uri, kS3Prefix_))
   {
      auto uriParts = s3Uri.substr(kS3Prefix_.size()).find('/');
      if (uriParts != std::string::npos)
      {
         return s3Uri.substr(kS3Prefix_.size(), uriParts - kS3Prefix_.size());
      }
      else
      {
         return s3Uri.substr(kS3Prefix_.size());
      }
   }
   return {};
}

} // namespace scwx::provider
