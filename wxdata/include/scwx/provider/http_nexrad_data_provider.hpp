#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>

namespace scwx::provider
{

class HttpNexradDataProvider : public NexradDataProvider
{
public:
   explicit HttpNexradDataProvider(const std::string& radarSite,
                                   const std::string& baseUri);
   virtual ~HttpNexradDataProvider();

   HttpNexradDataProvider(const HttpNexradDataProvider&)            = delete;
   HttpNexradDataProvider& operator=(const HttpNexradDataProvider&) = delete;

   HttpNexradDataProvider(HttpNexradDataProvider&&) noexcept;
   HttpNexradDataProvider& operator=(HttpNexradDataProvider&&) noexcept;

   // NexradDataProvider interface implementation
   [[nodiscard]] std::size_t cache_size() const override;
   [[nodiscard]] std::chrono::system_clock::time_point
                                      last_modified() const override;
   [[nodiscard]] std::chrono::seconds update_period() const override;

   std::string FindKey(std::chrono::system_clock::time_point time) override;
   std::string FindLatestKey() override;
   std::chrono::system_clock::time_point FindLatestTime() override;

   std::vector<std::chrono::system_clock::time_point>
        GetTimePointsByDate(std::chrono::system_clock::time_point date,
                            bool                                  update) override;
   bool IsDateCached(std::chrono::system_clock::time_point date) override;

   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;

   std::pair<size_t, size_t> Refresh() override;
   void                      Shutdown() noexcept override;

protected:
   // HTTP operations for derived classes
   std::string       DownloadToString(const std::string& url);
   std::stringstream DownloadToStream(const std::string& url);

   // Derived classes must implement these
   virtual std::string
   GetListingUrl(std::chrono::system_clock::time_point date) = 0;
   virtual std::string GetFileUrl(const std::string& key)    = 0;

   // List and parse directory, add to cache. Returns (success, newObjects,
   // totalObjects)
   std::tuple<bool, std::size_t, std::size_t>
   ListObjects(std::chrono::system_clock::time_point date) override = 0;

   // Access to internal state for derived classes
   bool               AddToCache(std::chrono::system_clock::time_point time,
                                 const std::string&                    key,
                                 std::chrono::system_clock::time_point lastModified);
   void               ResetCacheStart();
   void               ResetCacheFinish();
   [[nodiscard]] bool IsRunning() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
