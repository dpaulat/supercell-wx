#pragma once

#include <scwx/provider/level2_data_provider.hpp>

namespace scwx
{
namespace provider
{

class AwsLevel2DataProviderImpl;

/**
 * @brief AWS Level 2 Data Provider
 */
class AwsLevel2DataProvider : public Level2DataProvider
{
public:
   explicit AwsLevel2DataProvider(const std::string& radarSite);
   explicit AwsLevel2DataProvider(const std::string& radarSite,
                                  const std::string& bucketName,
                                  const std::string& region);
   ~AwsLevel2DataProvider();

   AwsLevel2DataProvider(const AwsLevel2DataProvider&) = delete;
   AwsLevel2DataProvider& operator=(const AwsLevel2DataProvider&) = delete;

   AwsLevel2DataProvider(AwsLevel2DataProvider&&) noexcept;
   AwsLevel2DataProvider& operator=(AwsLevel2DataProvider&&) noexcept;

   size_t cache_size() const;

   std::chrono::system_clock::time_point last_modified() const;
   std::chrono::seconds                  update_period() const;

   std::string FindKey(std::chrono::system_clock::time_point time);
   std::string FindLatestKey();
   std::pair<size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date);
   std::shared_ptr<wsr88d::Ar2vFile> LoadObjectByKey(const std::string& key);
   size_t                            Refresh();

   std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const;

   static std::chrono::system_clock::time_point
   GetTimePointFromKey(const std::string& key);

private:
   std::unique_ptr<AwsLevel2DataProviderImpl> p;
};

} // namespace provider
} // namespace scwx
