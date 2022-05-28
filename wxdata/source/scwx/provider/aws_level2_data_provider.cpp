#include <scwx/provider/aws_level2_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/time.hpp>
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
   struct ObjectRecord
   {
      explicit ObjectRecord(
         const std::string&                    key,
         std::chrono::system_clock::time_point lastModified) :
          key_ {key}, lastModified_ {lastModified}
      {
      }
      ~ObjectRecord() = default;

      std::string                           key_;
      std::chrono::system_clock::time_point lastModified_;
   };

   explicit AwsLevel2DataProviderImpl(const std::string& radarSite,
                                      const std::string& bucketName,
                                      const std::string& region) :
       radarSite_ {radarSite},
       bucketName_ {bucketName},
       region_ {region},
       client_ {nullptr},
       objects_ {},
       objectsMutex_ {},
       lastModified_ {},
       updatePeriod_ {}
   {
      Aws::Client::ClientConfiguration config;
      config.region = region_;

      client_ = std::make_unique<Aws::S3::S3Client>(config);
   }

   ~AwsLevel2DataProviderImpl() {}

   void UpdateMetadata();

   std::string radarSite_;
   std::string bucketName_;
   std::string region_;

   std::unique_ptr<Aws::S3::S3Client> client_;

   std::map<std::chrono::system_clock::time_point, ObjectRecord> objects_;
   std::shared_mutex                                             objectsMutex_;

   std::chrono::system_clock::time_point lastModified_;
   std::chrono::seconds                  updatePeriod_;
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

size_t AwsLevel2DataProvider::cache_size() const
{
   return p->objects_.size();
}

std::chrono::seconds AwsLevel2DataProvider::update_period() const
{
   return p->updatePeriod_;
}

std::chrono::system_clock::time_point
AwsLevel2DataProvider::last_modified() const
{
   return p->lastModified_;
}

std::string
AwsLevel2DataProvider::FindKey(std::chrono::system_clock::time_point time)
{
   logger_->debug("FindKey: {}", util::TimeString(time));

   std::string key {};

   std::shared_lock lock(p->objectsMutex_);

   auto element = util::GetBoundedElement(p->objects_, time);

   if (element.has_value())
   {
      key = element->key_;
   }

   return key;
}

std::string AwsLevel2DataProvider::FindLatestKey()
{
   logger_->debug("FindLatestKey()");

   std::string key {};

   std::shared_lock lock(p->objectsMutex_);

   if (!p->objects_.empty())
   {
      key = p->objects_.crbegin()->second.key_;
   }

   return key;
}

std::pair<size_t, size_t>
AwsLevel2DataProvider::ListObjects(std::chrono::system_clock::time_point date)
{
   const std::string prefix =
      fmt::format("{0:%Y/%m/%d}/{1}/", fmt::gmtime(date), p->radarSite_);

   logger_->debug("ListObjects: {}", prefix);

   Aws::S3::Model::ListObjectsV2Request request;
   request.SetBucket(p->bucketName_);
   request.SetPrefix(prefix);

   auto outcome = p->client_->ListObjectsV2(request);

   size_t newObjects   = 0;
   size_t totalObjects = 0;

   if (outcome.IsSuccess())
   {
      auto& objects = outcome.GetResult().GetContents();

      logger_->debug("Found {} objects", objects.size());

      // Store objects
      std::for_each( //
         objects.cbegin(),
         objects.cend(),
         [&](const Aws::S3::Model::Object& object)
         {
            std::string key = object.GetKey();

            if (!key.ends_with("_MDM"))
            {
               auto time = GetTimePointFromKey(key);

               std::chrono::seconds lastModifiedSeconds {
                  object.GetLastModified().Seconds()};
               std::chrono::system_clock::time_point lastModified {
                  lastModifiedSeconds};

               std::unique_lock lock(p->objectsMutex_);

               auto [it, inserted] = p->objects_.insert_or_assign(
                  time,
                  AwsLevel2DataProviderImpl::ObjectRecord {key, lastModified});

               if (inserted)
               {
                  newObjects++;
               }

               totalObjects++;
            }
         });
   }
   else
   {
      logger_->warn("Could not list objects: {}",
                    outcome.GetError().GetMessage());
   }

   p->UpdateMetadata();

   return std::make_pair(newObjects, totalObjects);
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

size_t AwsLevel2DataProvider::Refresh()
{
   using namespace std::chrono;

   logger_->debug("Refresh()");

   static std::mutex               refreshMutex;
   static system_clock::time_point refreshDate {};

   auto today     = floor<days>(system_clock::now());
   auto yesterday = today - days {1};

   std::unique_lock lock(refreshMutex);

   size_t totalNewObjects = 0;

   // If we haven't gotten any objects from today, first list objects for
   // yesterday, to ensure we haven't missed any objects near midnight
   if (refreshDate < today)
   {
      auto [newObjects, totalObjects] = ListObjects(yesterday);
      totalNewObjects                 = newObjects;
      if (totalObjects > 0)
      {
         refreshDate = yesterday;
      }
   }

   auto [newObjects, totalObjects] = ListObjects(today);
   totalNewObjects += newObjects;
   if (totalObjects > 0)
   {
      refreshDate = today;
   }

   return totalNewObjects;
}

void AwsLevel2DataProviderImpl::UpdateMetadata()
{
   std::shared_lock lock(objectsMutex_);

   if (!objects_.empty())
   {
      lastModified_ = objects_.crbegin()->second.lastModified_;
   }

   if (objects_.size() >= 2)
   {
      auto it           = objects_.crbegin();
      auto lastModified = it->second.lastModified_;
      auto prevModified = (++it)->second.lastModified_;
      auto delta        = lastModified - prevModified;

      updatePeriod_ = std::chrono::duration_cast<std::chrono::seconds>(delta);
   }
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
