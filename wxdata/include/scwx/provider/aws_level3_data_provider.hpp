#pragma once

#include <scwx/provider/aws_nexrad_data_provider.hpp>

namespace scwx
{
namespace provider
{

/**
 * @brief AWS Level 3 Data Provider
 */
class AwsLevel3DataProvider : public AwsNexradDataProvider
{
public:
   explicit AwsLevel3DataProvider(const std::string& radarSite,
                                  const std::string& product);
   explicit AwsLevel3DataProvider(const std::string& radarSite,
                                  const std::string& product,
                                  const std::string& bucketName,
                                  const std::string& region);
   ~AwsLevel3DataProvider();

   AwsLevel3DataProvider(const AwsLevel3DataProvider&) = delete;
   AwsLevel3DataProvider& operator=(const AwsLevel3DataProvider&) = delete;

   AwsLevel3DataProvider(AwsLevel3DataProvider&&) noexcept;
   AwsLevel3DataProvider& operator=(AwsLevel3DataProvider&&) noexcept;

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
