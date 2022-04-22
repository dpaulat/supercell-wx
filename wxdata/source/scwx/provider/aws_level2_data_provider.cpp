#include <scwx/provider/aws_level2_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::aws_level2_data_provider";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static const std::string kDefaultBucketName_ = "noaa-nexrad-level2";
static const std::string kDefaultRegion_     = "us-east-1";

class AwsLevel2DataProviderImpl
{
public:
   explicit AwsLevel2DataProviderImpl(const std::string& radarSite,
                                      const std::string& bucketName,
                                      const std::string& region) :
       radarSite_ {radarSite},
       bucketName_ {bucketName},
       region_ {region},
       client_ {nullptr}
   {
      Aws::Client::ClientConfiguration config;
      config.region = region_;

      client_ = std::make_unique<Aws::S3::S3Client>(config);
   }

   ~AwsLevel2DataProviderImpl() {}

   std::string radarSite_;
   std::string bucketName_;
   std::string region_;

   std::unique_ptr<Aws::S3::S3Client> client_;
};

AwsLevel2DataProvider::AwsLevel2DataProvider(const std::string& radarSite) :
    AwsLevel2DataProvider(radarSite, kDefaultBucketName_, kDefaultRegion_)
{
}
AwsLevel2DataProvider::AwsLevel2DataProvider(const std::string& radarSite,
                                             const std::string& bucketName,
                                             const std::string& region) :
    p(std::make_unique<AwsLevel2DataProviderImpl>(
       radarSite, bucketName, region))
{
}
AwsLevel2DataProvider::~AwsLevel2DataProvider() = default;

AwsLevel2DataProvider::AwsLevel2DataProvider(AwsLevel2DataProvider&&) noexcept =
   default;
AwsLevel2DataProvider&
AwsLevel2DataProvider::operator=(AwsLevel2DataProvider&&) noexcept = default;

void AwsLevel2DataProvider::ListObjects(
   std::chrono::system_clock::time_point date)
{
   const std::string prefix =
      fmt::format("{0:%Y/%m/%d}/{1}/", date, p->radarSite_);

   logger_->debug("ListObjects: {}", prefix);

   Aws::S3::Model::ListObjectsV2Request request;
   request.SetBucket(p->bucketName_);
   request.SetPrefix(prefix);

   auto outcome = p->client_->ListObjectsV2(request);

   if (outcome.IsSuccess())
   {
      auto& objects = outcome.GetResult().GetContents();

      logger_->debug("Found {} objects", objects.size());

      // TODO: Store
   }
   else
   {
      logger_->warn("Could not list objects: {}",
                    outcome.GetError().GetMessage());
   }
}

std::shared_ptr<wsr88d::Ar2vFile>
AwsLevel2DataProvider::LoadObjectByKey(const std::string& key)
{
   std::shared_ptr<wsr88d::Ar2vFile> level2File = nullptr;

   Aws::S3::Model::GetObjectRequest request;
   request.SetBucket(p->bucketName_);
   request.SetKey(key);

   auto outcome = p->client_->GetObject(request);

   if (outcome.IsSuccess())
   {
      auto& body = outcome.GetResultWithOwnership().GetBody();

      std::shared_ptr<wsr88d::NexradFile> nexradFile =
         wsr88d::NexradFileFactory::Create(body);

      level2File = std::dynamic_pointer_cast<wsr88d::Ar2vFile>(nexradFile);
   }
   else
   {
      logger_->warn("Could not get object: {}",
                    outcome.GetError().GetMessage());
   }

   return level2File;
}

void AwsLevel2DataProvider::Refresh()
{
   logger_->debug("Refresh()");

   // TODO: What if the date just rolled, we might miss from the previous date?
   ListObjects(std::chrono::system_clock::now());
}

} // namespace provider
} // namespace scwx
