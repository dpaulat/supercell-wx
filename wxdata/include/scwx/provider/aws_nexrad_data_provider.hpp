#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>

namespace Aws::S3
{
class S3Client;
} // namespace Aws::S3

namespace scwx::provider
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

   AwsNexradDataProvider(const AwsNexradDataProvider&)            = delete;
   AwsNexradDataProvider& operator=(const AwsNexradDataProvider&) = delete;

   AwsNexradDataProvider(AwsNexradDataProvider&&) noexcept;
   AwsNexradDataProvider& operator=(AwsNexradDataProvider&&) noexcept;

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
   [[nodiscard]] bool IsDateArchiveAvailable() const override;
   bool IsDateCached(std::chrono::system_clock::time_point date) override;
   std::tuple<bool, size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;
   std::pair<size_t, size_t> Refresh() override;

   /**
    * @brief Shuts down the provider and stops any in-progress network requests.
    */
   void Shutdown() noexcept override;

protected:
   std::shared_ptr<Aws::S3::S3Client> client();

   virtual std::string
   GetPrefix(std::chrono::system_clock::time_point date) = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
