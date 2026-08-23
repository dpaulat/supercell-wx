#include <scwx/qt/manager/product_datastore.hpp>
#include <scwx/qt/manager/provider_manager.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <execution>
#include <list>
#include <mutex>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <unordered_map>

namespace scwx::qt::manager
{

namespace
{

static const std::string logPrefix_ = "scwx::qt::manager::product_datastore";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

using RadarProductRecordList =
   std::list<std::shared_ptr<types::RadarProductRecord>>;

static constexpr std::size_t kMinimumCacheLimit_ {6u};

} // namespace

class ProductDatastore::Impl
{
public:
   void SetCacheLimit(std::size_t cacheLimit)
   {
      cacheLimit_.store(std::max<std::size_t>(cacheLimit, kMinimumCacheLimit_));
   }

   std::size_t cache_limit() const { return cacheLimit_.load(); }

   std::shared_ptr<types::RadarProductRecord>
   Store(const std::shared_ptr<types::RadarProductRecord>& record)
   {
      logger_->trace("Store()");

      if (record == nullptr)
      {
         return nullptr;
      }

      std::shared_ptr<types::RadarProductRecord> storedRecord = nullptr;

      const auto timeInSeconds =
         std::chrono::time_point_cast<std::chrono::seconds,
                                      std::chrono::system_clock>(
            record->time());

      if (record->radar_product_group() == common::RadarProductGroup::Level2)
      {
         std::unique_lock const lock {level2ProductRecordMutex_};

         auto it = level2ProductRecords_.find(timeInSeconds);
         if (it != level2ProductRecords_.cend())
         {
            storedRecord = it->second.lock();

            if (storedRecord != nullptr)
            {
               logger_->debug(
                  "Level 2 product previously loaded, loading from cache");
            }
         }

         if (storedRecord == nullptr)
         {
            storedRecord                         = record;
            level2ProductRecords_[timeInSeconds] = record;
         }

         UpdateRecentRecords(level2ProductRecentRecords_, storedRecord);
      }
      else if (record->radar_product_group() ==
               common::RadarProductGroup::Level3)
      {
         std::unique_lock const lock {level3ProductRecordMutex_};

         auto& productMap = level3ProductRecordsMap_[record->radar_product()];

         auto it = productMap.find(timeInSeconds);
         if (it != productMap.cend())
         {
            storedRecord = it->second.lock();

            if (storedRecord != nullptr)
            {
               logger_->debug(
                  "Level 3 product previously loaded, loading from cache");
            }
         }

         if (storedRecord == nullptr)
         {
            storedRecord              = record;
            productMap[timeInSeconds] = record;
         }

         UpdateRecentRecords(
            level3ProductRecentRecordsMap_[record->radar_product()],
            storedRecord);
      }
      else
      {
         logger_->debug(
            "Unsupported radar product group: {}",
            common::GetRadarProductGroupName(record->radar_product_group()));
      }

      return storedRecord;
   }

   void PopulateLevel2ProductTimes(
      const std::shared_ptr<ProviderManager>& level2ProviderManager,
      const std::shared_ptr<ProviderManager>& level2ChunksProviderManager,
      std::chrono::system_clock::time_point   time,
      bool                                    update)
   {
      PopulateProductTimes(level2ProviderManager,
                           level2ProductRecords_,
                           level2ProductRecordMutex_,
                           time,
                           update);
      PopulateProductTimes(level2ChunksProviderManager,
                           level2ProductRecords_,
                           level2ProductRecordMutex_,
                           time,
                           update);
   }

   void PopulateLevel3ProductTimes(
      const std::shared_ptr<ProviderManager>& level3ProviderManager,
      const std::string&                      product,
      std::chrono::system_clock::time_point   time,
      bool                                    update)
   {
      std::unique_lock level3ProductRecordLock {level3ProductRecordMutex_};
      auto&            level3ProductRecords = level3ProductRecordsMap_[product];
      level3ProductRecordLock.unlock();

      PopulateProductTimes(level3ProviderManager,
                           level3ProductRecords,
                           level3ProductRecordMutex_,
                           time,
                           update);
   }

   std::shared_ptr<wsr88d::NexradFile>
   GetCachedNexradFile(common::RadarProductGroup             group,
                       const std::string&                    level3Product,
                       std::chrono::system_clock::time_point time)
   {
      std::shared_ptr<types::RadarProductRecord> existingRecord = nullptr;

      const auto timeInSeconds =
         std::chrono::time_point_cast<std::chrono::seconds,
                                      std::chrono::system_clock>(time);

      if (group == common::RadarProductGroup::Level2)
      {
         std::shared_lock const sharedLock {level2ProductRecordMutex_};

         auto it = level2ProductRecords_.find(timeInSeconds);
         if (it != level2ProductRecords_.cend())
         {
            existingRecord = it->second.lock();

            if (existingRecord != nullptr)
            {
               logger_->trace(
                  "Data previously loaded, loading from data cache");
            }
         }
      }
      else if (group == common::RadarProductGroup::Level3)
      {
         std::shared_lock const sharedLock {level3ProductRecordMutex_};

         auto productIt = level3ProductRecordsMap_.find(level3Product);
         if (productIt != level3ProductRecordsMap_.cend())
         {
            auto it = productIt->second.find(timeInSeconds);
            if (it != productIt->second.cend())
            {
               existingRecord = it->second.lock();

               if (existingRecord != nullptr)
               {
                  logger_->trace(
                     "Data previously loaded, loading from data cache");
               }
            }
         }
      }

      if (existingRecord != nullptr)
      {
         return existingRecord->nexrad_file();
      }

      return nullptr;
   }

   std::vector<RadarProductRecordEntry>
   FindLevel2RecordEntries(std::chrono::system_clock::time_point time) const
   {
      std::vector<RadarProductRecordEntry> entries {};

      std::shared_lock const lock {level2ProductRecordMutex_};

      // If a default-initialized time point is given, return the latest record
      if (!level2ProductRecords_.empty() &&
          time == std::chrono::system_clock::time_point {})
      {
         const auto& recordEntry = *level2ProductRecords_.rbegin();
         entries.push_back({recordEntry.first, recordEntry.second});
      }
      else
      {
         // Get the requested record
         auto recordIt =
            scwx::util::GetBoundedElementIterator(level2ProductRecords_, time);

         if (recordIt != level2ProductRecords_.cend())
         {
            entries.push_back({recordIt->first, recordIt->second});

            // The requested time may be in the previous record, so get that too
            if (recordIt != level2ProductRecords_.cbegin())
            {
               const auto previousIt = std::prev(recordIt);
               entries.push_back({previousIt->first, previousIt->second});
            }
         }
      }

      return entries;
   }

   std::optional<RadarProductRecordEntry>
   FindLevel3RecordEntry(const std::string&                    product,
                         std::chrono::system_clock::time_point time) const
   {
      std::shared_lock const lock {level3ProductRecordMutex_};

      auto it = level3ProductRecordsMap_.find(product);
      if (it == level3ProductRecordsMap_.cend() || it->second.empty())
      {
         return std::nullopt;
      }

      // If a default-initialized time point is given, return the latest record
      if (time == std::chrono::system_clock::time_point {})
      {
         const auto& recordEntry = *it->second.rbegin();
         return RadarProductRecordEntry {recordEntry.first, recordEntry.second};
      }

      auto recordPtr = scwx::util::GetBoundedElementPointer(it->second, time);
      if (recordPtr == nullptr)
      {
         return std::nullopt;
      }

      return RadarProductRecordEntry {recordPtr->first, recordPtr->second};
   }

   void ForEachLevel2Record(
      const std::function<void(std::chrono::system_clock::time_point time,
                               bool expired)>& callback) const
   {
      std::shared_lock const lock {level2ProductRecordMutex_};

      for (auto& record : level2ProductRecords_)
      {
         callback(record.first, record.second.expired());
      }
   }

   void ForEachLevel3Product(
      const std::function<void(const std::string&           product,
                               const RadarProductRecordMap& recordMap)>&
         callback) const
   {
      std::shared_lock const lock {level3ProductRecordMutex_};

      for (auto& recordMap : level3ProductRecordsMap_)
      {
         callback(recordMap.first, recordMap.second);
      }
   }

private:
   void
   UpdateRecentRecords(RadarProductRecordList& recentList,
                       const std::shared_ptr<types::RadarProductRecord>& record)
   {
      const std::size_t recentListMaxSize {cacheLimit_.load()};
      bool              iteratorErased = false;

      auto it = std::find(recentList.cbegin(), recentList.cend(), record);
      if (it != recentList.cbegin() && it != recentList.cend())
      {
         // If the record exists beyond the front of the list, remove it
         recentList.erase(it);
         iteratorErased = true;
      }

      if (iteratorErased || recentList.size() == 0 || it != recentList.cbegin())
      {
         // Add the record to the front of the list, unless it's already there
         recentList.push_front(record);
      }

      while (recentList.size() > recentListMaxSize)
      {
         // Remove from the end of the list while it's too big
         recentList.pop_back();
      }
   }

   void
   PopulateProductTimes(const std::shared_ptr<ProviderManager>& providerManager,
                        RadarProductRecordMap& productRecordMap,
                        std::shared_mutex&     productRecordMutex,
                        std::chrono::system_clock::time_point time,
                        bool                                  update)
   {
      if (providerManager == nullptr)
      {
         return;
      }

      const auto providers = providerManager->providers();
      if (providers.empty())
      {
         return;
      }

      if (update)
      {
         logger_->debug(
            "Populating product times: {}, {}, {}",
            common::GetRadarProductGroupName(providerManager->group()),
            providerManager->product(),
            scwx::util::time::TimeString(time));
      }
      else
      {
         logger_->trace(
            "Populating cached product times: {}, {}, {}",
            common::GetRadarProductGroupName(providerManager->group()),
            providerManager->product(),
            scwx::util::time::TimeString(time));
      }

      auto today = std::chrono::floor<std::chrono::days>(time);

      // Assume a query for the epoch is a query for now
      if (today == std::chrono::system_clock::time_point {})
      {
         today = std::chrono::floor<std::chrono::days>(scwx::util::time::now());
      }

      const auto yesterday = today - std::chrono::days {1};
      const auto tomorrow  = today + std::chrono::days {1};
      const auto dates     = std::array {yesterday, today, tomorrow};

      std::set<std::chrono::system_clock::time_point> volumeTimes {};
      std::mutex                                      volumeTimesMutex {};

      const auto processDate =
         [&](const std::shared_ptr<provider::NexradDataProvider>& provider,
             const auto&                                          date)
      {
         // Don't query for a time point in the future
         if (date > scwx::util::time::now())
         {
            return;
         }

         // Query the provider for volume time points
         const auto timePoints = provider->GetTimePointsByDate(date, update);
         if (timePoints.empty())
         {
            return;
         }

         providerManager->NoteVolumeTimes(provider->radar_site(), timePoints);

         // Lock the merged volume time list
         const std::unique_lock volumeTimesLock {volumeTimesMutex};

         // Copy time points to the merged list
         std::copy(timePoints.begin(),
                   timePoints.end(),
                   std::inserter(volumeTimes, volumeTimes.end()));
      };

      std::for_each(
         std::execution::par,
         providers.begin(),
         providers.end(),
         [&](const auto& provider)
         {
            if (provider->IsDateArchiveAvailable())
            {
               // For yesterday, today and tomorrow (in parallel)
               std::for_each(std::execution::par,
                             dates.begin(),
                             dates.end(),
                             [&](const auto& date)
                             {
                                const auto candidates =
                                   common::GetRadarIdCandidates(
                                      provider->radar_site(), date);

                                if (std::ranges::find(candidates,
                                                      provider->radar_site()) !=
                                    candidates.cend())
                                {
                                   processDate(provider, date);
                                }
                             });
            }
            else
            {
               const auto candidates =
                  common::GetRadarIdCandidates(provider->radar_site(), today);

               if (std::ranges::find(candidates, provider->radar_site()) !=
                   candidates.cend())
               {
                  processDate(provider, today);
               }
            }
         });

      // Lock the product record map
      std::unique_lock const lock {productRecordMutex};

      // Merge volume times into map
      std::transform(
         volumeTimes.cbegin(),
         volumeTimes.cend(),
         std::inserter(productRecordMap, productRecordMap.begin()),
         [](const std::chrono::system_clock::time_point& volumeTime)
         {
            // Match Store()/GetCachedNexradFile() second-aligned keys
            const auto timeInSeconds =
               std::chrono::time_point_cast<std::chrono::seconds,
                                            std::chrono::system_clock>(
                  volumeTime);
            return std::pair<std::chrono::system_clock::time_point,
                             std::weak_ptr<types::RadarProductRecord>>(
               timeInSeconds, std::weak_ptr<types::RadarProductRecord> {});
         });
   }

   std::atomic<std::size_t> cacheLimit_ {kMinimumCacheLimit_};

   RadarProductRecordMap  level2ProductRecords_ {};
   RadarProductRecordList level2ProductRecentRecords_ {};
   std::unordered_map<std::string, RadarProductRecordMap>
      level3ProductRecordsMap_ {};
   std::unordered_map<std::string, RadarProductRecordList>
                             level3ProductRecentRecordsMap_ {};
   mutable std::shared_mutex level2ProductRecordMutex_ {};
   mutable std::shared_mutex level3ProductRecordMutex_ {};
};

ProductDatastore::ProductDatastore() : p(std::make_unique<Impl>()) {}

ProductDatastore::~ProductDatastore() = default;

void ProductDatastore::SetCacheLimit(std::size_t cacheLimit)
{
   p->SetCacheLimit(cacheLimit);
}

std::size_t ProductDatastore::cache_limit() const
{
   return p->cache_limit();
}

std::shared_ptr<types::RadarProductRecord> ProductDatastore::Store(
   const std::shared_ptr<types::RadarProductRecord>& record)
{
   return p->Store(record);
}

bool ProductDatastore::AreProductTimesPopulated(
   const std::shared_ptr<ProviderManager>& providerManager,
   std::chrono::system_clock::time_point   time)
{
   if (providerManager == nullptr)
   {
      return false;
   }

   const auto providers = providerManager->providers();
   if (providers.empty())
   {
      // If providers are not available, assume product times are populated
      return true;
   }

   auto today = std::chrono::floor<std::chrono::days>(time);

   bool productTimesPopulated = false;

   // Assume a query for the epoch is a query for now
   if (today == std::chrono::system_clock::time_point {})
   {
      today = std::chrono::floor<std::chrono::days>(scwx::util::time::now());
   }

   const auto yesterday = today - std::chrono::days {1};
   const auto tomorrow  = today + std::chrono::days {1};

   for (const auto& provider : providers)
   {
      bool providerTimesPopulated = true;
      bool providerValidForDates  = false;

      if (provider->IsDateArchiveAvailable())
      {
         const auto dates = std::array {yesterday, today, tomorrow};

         for (const auto& date : dates)
         {
            // Don't query for a time point in the future
            if (date > scwx::util::time::now())
            {
               continue;
            }

            const auto candidates =
               common::GetRadarIdCandidates(provider->radar_site(), date);

            // Skip dates outside this provider's candidate window
            if (std::ranges::find(candidates, provider->radar_site()) ==
                candidates.cend())
            {
               continue;
            }

            providerValidForDates = true;

            if (!provider->IsDateCached(date))
            {
               providerTimesPopulated = false;
            }
         }
      }
      else
      {
         const auto candidates =
            common::GetRadarIdCandidates(provider->radar_site(), today);

         if (std::ranges::find(candidates, provider->radar_site()) !=
             candidates.cend())
         {
            providerValidForDates = true;
         }

         if (providerValidForDates && !provider->IsDateCached(today))
         {
            providerTimesPopulated = false;
         }
      }

      if (providerValidForDates && providerTimesPopulated)
      {
         productTimesPopulated = true;
         break;
      }
   }

   return productTimesPopulated;
}

void ProductDatastore::PopulateLevel2ProductTimes(
   const std::shared_ptr<ProviderManager>& level2ProviderManager,
   const std::shared_ptr<ProviderManager>& level2ChunksProviderManager,
   std::chrono::system_clock::time_point   time,
   bool                                    update)
{
   p->PopulateLevel2ProductTimes(
      level2ProviderManager, level2ChunksProviderManager, time, update);
}

void ProductDatastore::PopulateLevel3ProductTimes(
   const std::shared_ptr<ProviderManager>& level3ProviderManager,
   const std::string&                      product,
   std::chrono::system_clock::time_point   time,
   bool                                    update)
{
   p->PopulateLevel3ProductTimes(level3ProviderManager, product, time, update);
}

std::shared_ptr<wsr88d::NexradFile> ProductDatastore::GetCachedNexradFile(
   common::RadarProductGroup             group,
   const std::string&                    level3Product,
   std::chrono::system_clock::time_point time)
{
   return p->GetCachedNexradFile(group, level3Product, time);
}

std::vector<RadarProductRecordEntry> ProductDatastore::FindLevel2RecordEntries(
   std::chrono::system_clock::time_point time) const
{
   return p->FindLevel2RecordEntries(time);
}

std::optional<RadarProductRecordEntry> ProductDatastore::FindLevel3RecordEntry(
   const std::string& product, std::chrono::system_clock::time_point time) const
{
   return p->FindLevel3RecordEntry(product, time);
}

void ProductDatastore::ForEachLevel2Record(
   const std::function<void(std::chrono::system_clock::time_point time,
                            bool expired)>& callback) const
{
   p->ForEachLevel2Record(callback);
}

void ProductDatastore::ForEachLevel3Product(
   const std::function<void(const std::string&           product,
                            const RadarProductRecordMap& recordMap)>& callback)
   const
{
   p->ForEachLevel3Product(callback);
}

} // namespace scwx::qt::manager
