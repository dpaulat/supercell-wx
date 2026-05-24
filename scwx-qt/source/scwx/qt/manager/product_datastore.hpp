#pragma once

#include <scwx/common/products.hpp>
#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/wsr88d/nexrad_file.hpp>

#include <chrono>
#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace scwx::qt::manager
{

class ProviderManager;

using RadarProductRecordMap =
   std::map<std::chrono::system_clock::time_point,
            std::weak_ptr<types::RadarProductRecord>>;
using RadarProductRecordList =
   std::list<std::shared_ptr<types::RadarProductRecord>>;

struct RadarProductRecordEntry
{
   std::chrono::system_clock::time_point    time {};
   std::weak_ptr<types::RadarProductRecord> record {};
};

class ProductDatastore
{
public:
   ProductDatastore() = default;

   void                      SetCacheLimit(std::size_t cacheLimit);
   [[nodiscard]] std::size_t cache_limit() const;

   std::shared_ptr<types::RadarProductRecord>
   Store(std::shared_ptr<types::RadarProductRecord> record);

   static bool AreProductTimesPopulated(
      const std::shared_ptr<ProviderManager>& providerManager,
      std::chrono::system_clock::time_point   time);

   void PopulateLevel2ProductTimes(
      const std::shared_ptr<ProviderManager>& level2ProviderManager,
      const std::shared_ptr<ProviderManager>& level2ChunksProviderManager,
      std::chrono::system_clock::time_point   time,
      bool                                    update = true);

   void PopulateLevel3ProductTimes(
      const std::shared_ptr<ProviderManager>& level3ProviderManager,
      const std::string&                      product,
      std::chrono::system_clock::time_point   time,
      bool                                    update = true);

   [[nodiscard]] std::shared_ptr<wsr88d::NexradFile>
   GetCachedNexradFile(common::RadarProductGroup             group,
                       const std::string&                    level3Product,
                       std::chrono::system_clock::time_point time);

   [[nodiscard]] std::vector<RadarProductRecordEntry>
   FindLevel2RecordEntries(std::chrono::system_clock::time_point time) const;

   [[nodiscard]] std::optional<RadarProductRecordEntry>
   FindLevel3RecordEntry(const std::string&                    product,
                         std::chrono::system_clock::time_point time) const;

   void ForEachLevel2Record(
      const std::function<void(std::chrono::system_clock::time_point time,
                               bool expired)>& callback) const;

   void ForEachLevel3Product(
      const std::function<void(const std::string&           product,
                               const RadarProductRecordMap& recordMap)>&
         callback) const;

private:
   void UpdateRecentRecords(RadarProductRecordList& recentList,
                            std::shared_ptr<types::RadarProductRecord> record);

   void
   PopulateProductTimes(const std::shared_ptr<ProviderManager>& providerManager,
                        RadarProductRecordMap& productRecordMap,
                        std::shared_mutex&     productRecordMutex,
                        std::chrono::system_clock::time_point time,
                        bool                                  update);

   std::atomic<std::size_t> cacheLimit_ {6u};

   RadarProductRecordMap  level2ProductRecords_ {};
   RadarProductRecordList level2ProductRecentRecords_ {};
   std::unordered_map<std::string, RadarProductRecordMap>
      level3ProductRecordsMap_ {};
   std::unordered_map<std::string, RadarProductRecordList>
                             level3ProductRecentRecordsMap_ {};
   mutable std::shared_mutex level2ProductRecordMutex_ {};
   mutable std::shared_mutex level3ProductRecordMutex_ {};
};

} // namespace scwx::qt::manager
