#pragma once

#include <scwx/provider/aws_nexrad_data_provider.hpp>

namespace scwx
{
namespace provider
{

/**
 * @brief AWS Level 2 Data Provider
 */
class AwsLevel2DataProvider : public AwsNexradDataProvider
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

   std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const;

   static std::chrono::system_clock::time_point
   GetTimePointFromKey(const std::string& key);

protected:
   std::string GetPrefix(std::chrono::system_clock::time_point date);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace provider
} // namespace scwx
