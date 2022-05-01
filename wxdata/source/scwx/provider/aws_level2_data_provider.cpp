#include <scwx/provider/aws_level2_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <shared_mutex>

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
static const auto logger_ = util::Logger::Create(logPrefix_);

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
       client_ {nullptr},
       objects_ {},
       objectsMutex_ {}
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

   std::map<std::chrono::system_clock::time_point, std::string> objects_;
   std::shared_mutex                                            objectsMutex_;
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

      // Store objects
      std::for_each(objects.cbegin(),
                    objects.cend(),
                    [&](const Aws::S3::Model::Object& object)
                    {
                       // TODO: Skip MDM
                       std::string key  = object.GetKey();
                       auto        time = GetTimePointFromKey(key);

                       std::unique_lock lock(p->objectsMutex_);

                       p->objects_[time] = key;
                    });
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

std::chrono::system_clock::time_point
AwsLevel2DataProvider::GetTimePointFromKey(const std::string& key)
{
   std::chrono::system_clock::time_point time {};

   const size_t lastSeparator = key.rfind('/');
   const size_t offset =
      (lastSeparator == std::string::npos) ? 0 : lastSeparator + 5;

   // Filename format is GGGGYYYYMMDD_TTTTTT(_V##).gz
   static constexpr size_t formatSize = std::string("YYYYMMDD_TTTTTT").size();

   if (key.size() >= offset + formatSize)
   {
      using namespace std::chrono;

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
