#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>

#include <aws/s3/S3Client.h>

namespace scwx
{
namespace provider
{

/**
 * @brief AWS NEXRAD Data Provider
 */
class AwsNexradDataProvider : public NexradDataProvider
{
public:
   explicit AwsNexradDataProvider(const std::string& radarSite,
                                  const std::string& bucketName,
                                  const std::string& region);
   virtual ~AwsNexradDataProvider();

   AwsNexradDataProvider(const AwsNexradDataProvider&) = delete;
   AwsNexradDataProvider& operator=(const AwsNexradDataProvider&) = delete;

   AwsNexradDataProvider(AwsNexradDataProvider&&) noexcept;
   AwsNexradDataProvider& operator=(AwsNexradDataProvider&&) noexcept;

   size_t cache_size() const;

   std::chrono::system_clock::time_point last_modified() const;
   std::chrono::seconds                  update_period() const;

   std::string FindKey(std::chrono::system_clock::time_point time);
   std::string FindLatestKey();
   std::pair<size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date);
   std::shared_ptr<wsr88d::NexradFile> LoadObjectByKey(const std::string& key);
   std::pair<size_t, size_t>           Refresh();

protected:
   std::shared_ptr<Aws::S3::S3Client> client();

   virtual std::string
   GetPrefix(std::chrono::system_clock::time_point date) = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace provider
} // namespace scwx
