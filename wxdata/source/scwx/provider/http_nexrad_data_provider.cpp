#include <algorithm>
#include <scwx/provider/http_nexrad_data_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <atomic>
#include <shared_mutex>
#include <string>
#include <utility>

#include <cpr/cpr.h>

namespace scwx::provider
{

static const std::string logPrefix_ =
   "scwx::provider::http_nexrad_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

class HttpNexradDataProvider::Impl
{
public:
   struct ObjectRecord
   {
      std::string                           key_;
      std::chrono::system_clock::time_point lastModified_;
   };

   explicit Impl(HttpNexradDataProvider* self,
                 std::string             baseUri,
                 std::string             radarSite) :
       self_ {self},
       baseUri_ {std::move(baseUri)},
       radarSite_ {std::move(radarSite)}
   {
   }
   ~Impl() { running_ = false; }
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void CheckDataPresent(std::chrono::system_clock::time_point date,
                         bool                                  update);
   void UpdateObjectDates(std::chrono::system_clock::time_point date);

   HttpNexradDataProvider* self_;

   std::string baseUri_;
   std::string radarSite_;

   bool dateArchiveAvailable_ {false};

   std::map<std::chrono::system_clock::time_point, ObjectRecord> objects_ {};
   std::map<std::chrono::system_clock::time_point, ObjectRecord> newObjects_ {};
   std::shared_mutex                                objectsMutex_ {};
   std::list<std::chrono::system_clock::time_point> objectDates_ {};
   std::atomic<bool>                                cacheResetting_ {false};

   std::mutex                            refreshMutex_ {};
   std::chrono::system_clock::time_point refreshDate_ {};
   std::chrono::system_clock::time_point lastModified_ {};
   std::chrono::seconds                  updatePeriod_ {};

   std::atomic<bool> running_ {true};
};

HttpNexradDataProvider::HttpNexradDataProvider(const std::string& radarSite,
                                               const std::string& baseUri) :
    p(std::make_unique<Impl>(this, baseUri, radarSite))
{
}

HttpNexradDataProvider::~HttpNexradDataProvider() = default;

HttpNexradDataProvider::HttpNexradDataProvider(
   HttpNexradDataProvider&&) noexcept = default;
HttpNexradDataProvider&
HttpNexradDataProvider::operator=(HttpNexradDataProvider&&) noexcept = default;

std::size_t HttpNexradDataProvider::cache_size() const
{
   std::shared_lock lock(p->objectsMutex_);
   return p->objects_.size();
}

std::chrono::system_clock::time_point
HttpNexradDataProvider::last_modified() const
{
   return p->lastModified_;
}

std::chrono::seconds HttpNexradDataProvider::update_period() const
{
   return p->updatePeriod_;
}

std::string
HttpNexradDataProvider::FindKey(std::chrono::system_clock::time_point time)
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

std::string HttpNexradDataProvider::FindLatestKey()
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

std::chrono::system_clock::time_point HttpNexradDataProvider::FindLatestTime()
{
   return GetTimePointByKey(FindLatestKey());
}

std::vector<std::chrono::system_clock::time_point>
HttpNexradDataProvider::GetTimePointsByDate(
   std::chrono::system_clock::time_point date, bool update)
{
   const auto day = std::chrono::floor<std::chrono::days>(date);

   std::vector<std::chrono::system_clock::time_point> timePoints {};

   logger_->trace("GetTimePointsByDate: {}", util::TimeString(date));

   p->CheckDataPresent(date, update);

   std::shared_lock lock(p->objectsMutex_);

   // Determine objects to retrieve
   const auto objectsBegin = p->objects_.lower_bound(day);
   const auto objectsEnd = p->objects_.lower_bound(day + std::chrono::days {1});

   // Copy time points to destination vector
   std::transform(objectsBegin,
                  objectsEnd,
                  std::back_inserter(timePoints),
                  [](const auto& object) { return object.first; });

   // Unlock mutex, finished
   lock.unlock();

   return timePoints;
}

bool HttpNexradDataProvider::IsDateCached(
   std::chrono::system_clock::time_point date)
{
   const auto day         = std::chrono::floor<std::chrono::days>(date);
   bool       dataPresent = false;

   const std::shared_lock lock(p->objectsMutex_);

   if (p->dateArchiveAvailable_)
   {
      // Is the date present in the date list?
      const auto currentDateIterator =
         std::find(p->objectDates_.cbegin(), p->objectDates_.cend(), day);

      dataPresent = currentDateIterator != p->objectDates_.cend();
   }
   else
   {
      // Is data present?
      dataPresent = !p->objects_.empty();
   }

   return dataPresent;
}

std::shared_ptr<wsr88d::NexradFile>
HttpNexradDataProvider::LoadObjectByKey(const std::string& key)
{
   if (!p->running_)
   {
      return nullptr;
   }

   const std::string fileUrl = GetFileUrl(key);
   std::stringstream ss      = DownloadToStream(fileUrl);

   // If the file is empty, return nullptr
   ss.seekg(0, std::ios::end);
   if (ss.tellg() == 0)
   {
      return nullptr;
   }
   ss.seekg(0, std::ios::beg);

   return wsr88d::NexradFileFactory::Create(ss);
}

std::shared_ptr<wsr88d::NexradFile> HttpNexradDataProvider::LoadObjectByTime(
   std::chrono::system_clock::time_point time)
{
   const std::string key = FindKey(time);
   if (key.empty())
   {
      return nullptr;
   }
   else
   {
      return LoadObjectByKey(key);
   }
}

std::pair<size_t, size_t> HttpNexradDataProvider::Refresh()
{
   using namespace std::chrono;

   logger_->debug("Refresh()");

   const auto today = floor<days>(util::time::now());

   const std::unique_lock lock(p->refreshMutex_);

   std::size_t allNewObjects   = 0;
   std::size_t allTotalObjects = 0;

   // If we haven't gotten any objects from today, first list objects for
   // yesterday, to ensure we haven't missed any objects near midnight
   if (p->refreshDate_ < today && p->dateArchiveAvailable_)
   {
      const auto yesterday                           = today - days {1};
      const auto [success, newObjects, totalObjects] = ListObjects(yesterday);
      allNewObjects                                  = newObjects;
      allTotalObjects                                = totalObjects;
      if (totalObjects > 0)
      {
         p->refreshDate_ = yesterday;
      }
   }

   const auto [success, newObjects, totalObjects] = ListObjects(today);
   allNewObjects += newObjects;
   allTotalObjects += totalObjects;
   if (totalObjects > 0)
   {
      p->refreshDate_ = today;
   }

   return std::make_pair(allNewObjects, allTotalObjects);
}

void HttpNexradDataProvider::Impl::CheckDataPresent(
   std::chrono::system_clock::time_point date, bool update)
{
   const auto day = std::chrono::floor<std::chrono::days>(date);

   if (dateArchiveAvailable_)
   {
      std::shared_lock lock(objectsMutex_);

      // Is the date present in the date list?
      const auto currentDateIterator = std::ranges::find(objectDates_, day);
      if (currentDateIterator == objectDates_.cend())
      {
         lock.unlock();

         if (update)
         {
            // List objects, since the date is not present in the date list
            const auto [success, newObjects, totalObjects] =
               self_->ListObjects(date);
            if (success)
            {
               UpdateObjectDates(date);
            }
         }
      }
      else
      {
         lock.unlock();

         // If we haven't updated the most recently queried dates yet, because
         // the date was already cached, update
         UpdateObjectDates(date);
      }
   }
   else
   {
      if (update)
      {
         std::shared_lock lock(objectsMutex_);

         // Is data present?
         if (objects_.empty())
         {
            lock.unlock();

            // List objects, since no data is present
            self_->ListObjects(date);
         }
      }
   }
}

void HttpNexradDataProvider::Impl::UpdateObjectDates(
   std::chrono::system_clock::time_point date)
{
   const auto day = std::chrono::floor<std::chrono::days>(date);

   std::unique_lock lock(objectsMutex_);

   // Remove any existing occurrences of day, and add to the back of the list
   objectDates_.remove(day);
   objectDates_.emplace_back(day);
}

void HttpNexradDataProvider::Shutdown() noexcept
{
   p->running_ = false;
}

std::string HttpNexradDataProvider::DownloadToString(const std::string& url)
{
   // Use CPR to download file
   ::cpr::Response response =
      ::cpr::Get(::cpr::Url {url},
                 network::cpr::GetHeader(),
                 network::cpr::GetDefaultTimeout(),
                 network::cpr::GetDefaultConnectTimeout(),
                 network::cpr::GetDefaultLowSpeed(),
                 network::cpr::GetDefaultProgressCallback(p->running_));

   if (response.status_code != ::cpr::status::HTTP_OK)
   {
      logger_->warn("Failed to download {}: {} ({})",
                    url,
                    response.error.message,
                    response.status_code);
      return {};
   }

   return response.text;
}

std::stringstream
HttpNexradDataProvider::DownloadToStream(const std::string& url)
{
   // Convert response to stream
   std::stringstream ss {DownloadToString(url), std::ios::in | std::ios::binary};
   return ss;
}

bool HttpNexradDataProvider::AddToCache(
   std::chrono::system_clock::time_point time,
   const std::string&                    key,
   std::chrono::system_clock::time_point lastModified)
{
   const std::unique_lock lock(p->objectsMutex_);

   Impl::ObjectRecord record {.key_ = key, .lastModified_ = lastModified};

   bool newObject = false;

   if (p->cacheResetting_)
   {
      newObject = !p->objects_.contains(time);
      p->newObjects_.insert_or_assign(time, record);
   }
   else
   {
      newObject = p->objects_.insert_or_assign(time, record).second;
   }

   return newObject;
}

void HttpNexradDataProvider::ResetCacheStart()
{
   p->cacheResetting_ = true;
}

void HttpNexradDataProvider::ResetCacheFinish()
{
   if (p->cacheResetting_)
   {
      const std::unique_lock lock(p->objectsMutex_);

      p->objects_ = std::move(p->newObjects_);
      p->newObjects_.clear();
   }

   p->cacheResetting_ = false;
}

} // namespace scwx::provider
