#pragma once

#include <scwx/provider/http_nexrad_data_provider.hpp>

namespace scwx::provider
{

class OndasLevel2DataProvider : public HttpNexradDataProvider
{
public:
   explicit OndasLevel2DataProvider(const std::string& radarSite,
                                    const std::string& baseUri);
   ~OndasLevel2DataProvider() override;

   OndasLevel2DataProvider(const OndasLevel2DataProvider&)            = delete;
   OndasLevel2DataProvider& operator=(const OndasLevel2DataProvider&) = delete;

   OndasLevel2DataProvider(OndasLevel2DataProvider&&) noexcept;
   OndasLevel2DataProvider& operator=(OndasLevel2DataProvider&&) noexcept;

   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   static std::chrono::system_clock::time_point
   GetTimePointFromKey(const std::string& key);

   std::tuple<bool, size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) override;

protected:
   std::string GetListingUrl(std::chrono::system_clock::time_point date);
   std::string GetFileUrl(const std::string& key) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
