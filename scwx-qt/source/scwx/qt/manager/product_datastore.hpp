#pragma once

#include <scwx/common/products.hpp>
#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/wsr88d/nexrad_file.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace scwx::qt::manager
{

class ProviderManager;

using RadarProductRecordMap =
   std::map<std::chrono::system_clock::time_point,
            std::weak_ptr<types::RadarProductRecord>>;

struct RadarProductRecordEntry
{
   std::chrono::system_clock::time_point    time {};
   std::weak_ptr<types::RadarProductRecord> record {};
};

class ProductDatastore
{
public:
   ProductDatastore();
   ~ProductDatastore();

   ProductDatastore(const ProductDatastore&)            = delete;
   ProductDatastore& operator=(const ProductDatastore&) = delete;
   ProductDatastore(ProductDatastore&&)                 = delete;
   ProductDatastore& operator=(ProductDatastore&&)      = delete;

   void                      SetCacheLimit(std::size_t cacheLimit);
   [[nodiscard]] std::size_t cache_limit() const;

   std::shared_ptr<types::RadarProductRecord>
   Store(const std::shared_ptr<types::RadarProductRecord>& record);

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
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
