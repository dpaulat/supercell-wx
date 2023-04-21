#include <scwx/provider/aws_nexrad_data_provider.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <shared_mutex>

#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::aws_nexrad_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

// Keep at least today, yesterday, and one more date
static const size_t kMinDatesBeforePruning_ = 4;
static const size_t kMaxObjects_            = 2500;

class AwsNexradDataProvider::Impl
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

   explicit Impl(const std::string& radarSite,
                 const std::string& bucketName,
                 const std::string& region) :
       radarSite_ {radarSite},
       bucketName_ {bucketName},
       region_ {region},
       client_ {nullptr},
       objects_ {},
       objectsMutex_ {},
       objectDates_ {},
       refreshMutex_ {},
       refreshDate_ {},
       lastModified_ {},
       updatePeriod_ {}
   {
      // Disable HTTP request for region
      util::SetEnvironment("AWS_EC2_METADATA_DISABLED", "true");

      Aws::Client::ClientConfiguration config;
      config.region = region_;

      client_ = std::make_shared<Aws::S3::S3Client>(config);
   }

   ~Impl() {}

   void PruneObjects();
   void UpdateMetadata();
   void UpdateObjectDates(std::chrono::system_clock::time_point date);

   std::string radarSite_;
   std::string bucketName_;
   std::string region_;

   std::shared_ptr<Aws::S3::S3Client> client_;

   std::map<std::chrono::system_clock::time_point, ObjectRecord> objects_;
   std::shared_mutex                                             objectsMutex_;
   std::list<std::chrono::system_clock::time_point>              objectDates_;

   std::mutex                            refreshMutex_;
   std::chrono::system_clock::time_point refreshDate_;

   std::chrono::system_clock::time_point lastModified_;
   std::chrono::seconds                  updatePeriod_;
};

AwsNexradDataProvider::AwsNexradDataProvider(const std::string& radarSite,
                                             const std::string& bucketName,
                                             const std::string& region) :
    p(std::make_unique<Impl>(radarSite, bucketName, region))
{
}
AwsNexradDataProvider::~AwsNexradDataProvider() = default;

AwsNexradDataProvider::AwsNexradDataProvider(AwsNexradDataProvider&&) noexcept =
   default;
AwsNexradDataProvider&
AwsNexradDataProvider::operator=(AwsNexradDataProvider&&) noexcept = default;

size_t AwsNexradDataProvider::cache_size() const
{
   return p->objects_.size();
}

std::shared_ptr<Aws::S3::S3Client> AwsNexradDataProvider::client()
{
   return p->client_;
}

std::chrono::seconds AwsNexradDataProvider::update_period() const
{
   return p->updatePeriod_;
}

std::chrono::system_clock::time_point
AwsNexradDataProvider::last_modified() const
{
   return p->lastModified_;
}

std::string
AwsNexradDataProvider::FindKey(std::chrono::system_clock::time_point time)
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

std::string AwsNexradDataProvider::FindLatestKey()
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
AwsNexradDataProvider::ListObjects(std::chrono::system_clock::time_point date)
{
   const std::string prefix {GetPrefix(date)};

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
               auto time = GetTimePointByKey(key);

               std::chrono::seconds lastModifiedSeconds {
                  object.GetLastModified().Seconds()};
               std::chrono::system_clock::time_point lastModified {
                  lastModifiedSeconds};

               std::unique_lock lock(p->objectsMutex_);

               auto [it, inserted] = p->objects_.insert_or_assign(
                  time, Impl::ObjectRecord {key, lastModified});

               if (inserted)
               {
                  newObjects++;
               }

               totalObjects++;
            }
         });

      if (newObjects > 0)
      {
         p->UpdateObjectDates(date);
         p->PruneObjects();
         p->UpdateMetadata();
      }
   }
   else
   {
      logger_->warn("Could not list objects: {}",
                    outcome.GetError().GetMessage());
   }

   return std::make_pair(newObjects, totalObjects);
}

std::shared_ptr<wsr88d::NexradFile>
AwsNexradDataProvider::LoadObjectByKey(const std::string& key)
{
   std::shared_ptr<wsr88d::NexradFile> nexradFile = nullptr;

   Aws::S3::Model::GetObjectRequest request;
   request.SetBucket(p->bucketName_);
   request.SetKey(key);

   auto outcome = p->client_->GetObject(request);

   if (outcome.IsSuccess())
   {
      auto& body = outcome.GetResultWithOwnership().GetBody();

      nexradFile = wsr88d::NexradFileFactory::Create(body);
   }
   else
   {
      logger_->warn("Could not get object: {}",
                    outcome.GetError().GetMessage());
   }

   return nexradFile;
}

std::pair<size_t, size_t> AwsNexradDataProvider::Refresh()
{
   using namespace std::chrono;

   logger_->debug("Refresh()");

   auto today     = floor<days>(system_clock::now());
   auto yesterday = today - days {1};

   std::unique_lock lock(p->refreshMutex_);

   size_t allNewObjects   = 0;
   size_t allTotalObjects = 0;

   // If we haven't gotten any objects from today, first list objects for
   // yesterday, to ensure we haven't missed any objects near midnight
   if (p->refreshDate_ < today)
   {
      auto [newObjects, totalObjects] = ListObjects(yesterday);
      allNewObjects                   = newObjects;
      allTotalObjects                 = totalObjects;
      if (totalObjects > 0)
      {
         p->refreshDate_ = yesterday;
      }
   }

   auto [newObjects, totalObjects] = ListObjects(today);
   allNewObjects += newObjects;
   allTotalObjects += totalObjects;
   if (totalObjects > 0)
   {
      p->refreshDate_ = today;
   }

   return std::make_pair(allNewObjects, allTotalObjects);
}

void AwsNexradDataProvider::Impl::PruneObjects()
{
   using namespace std::chrono;

   auto today     = floor<days>(system_clock::now());
   auto yesterday = today - days {1};

   std::unique_lock lock(objectsMutex_);

   for (auto it = objectDates_.cbegin();
        it != objectDates_.cend() && objects_.size() > kMaxObjects_ &&
        objectDates_.size() >= kMinDatesBeforePruning_;)
   {
      if (*it < yesterday)
      {
         // Erase oldest keys from objects list
         auto eraseBegin = objects_.lower_bound(*it);
         auto eraseEnd   = objects_.lower_bound(*it + days {1});
         objects_.erase(eraseBegin, eraseEnd);

         // Remove oldest date from object dates list
         it = objectDates_.erase(it);
      }
      else
      {
         ++it;
      }
   }
}

void AwsNexradDataProvider::Impl::UpdateMetadata()
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

void AwsNexradDataProvider::Impl::UpdateObjectDates(
   std::chrono::system_clock::time_point date)
{
   auto day = std::chrono::floor<std::chrono::days>(date);

   std::unique_lock lock(objectsMutex_);

   // Remove any existing occurrences of day, and add to the back of the list
   objectDates_.remove(day);
   objectDates_.push_back(day);
}

} // namespace provider
} // namespace scwx
