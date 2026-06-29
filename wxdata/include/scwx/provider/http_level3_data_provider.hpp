#pragma once

#include <scwx/provider/http_nexrad_data_provider.hpp>

namespace scwx::provider
{

class HttpLevel3DataProvider : public HttpNexradDataProvider
{
public:
   explicit HttpLevel3DataProvider(const std::string& radarSite,
                                   const std::string& product,
                                   const std::string& baseUri);
   virtual ~HttpLevel3DataProvider();

   void Shutdown() noexcept override;

   HttpLevel3DataProvider(const HttpLevel3DataProvider&)            = delete;
   HttpLevel3DataProvider& operator=(const HttpLevel3DataProvider&) = delete;

   HttpLevel3DataProvider(HttpLevel3DataProvider&&) noexcept;
   HttpLevel3DataProvider& operator=(HttpLevel3DataProvider&&) noexcept;

   std::tuple<bool, size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) override;

   std::string GetFileUrl(const std::string& key) override;
   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   void                     RequestAvailableProducts() override;
   std::vector<std::string> GetAvailableProducts() override;

protected:
   std::string GetListingUrl(std::chrono::system_clock::time_point date);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
