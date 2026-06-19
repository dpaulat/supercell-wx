#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>
#include <scwx/provider/aws_level2_data_provider.hpp>

#include <optional>

namespace Aws::S3
{
class S3Client;
} // namespace Aws::S3

namespace scwx::provider
{

/**
 * @brief AWS Level 2 Data Provider
 */
class AwsLevel2ChunksDataProvider : public NexradDataProvider
{
public:
   explicit AwsLevel2ChunksDataProvider(const std::string& radarSite);
   explicit AwsLevel2ChunksDataProvider(const std::string& radarSite,
                                        const std::string& bucketName,
                                        const std::string& region);
   ~AwsLevel2ChunksDataProvider() override;

   AwsLevel2ChunksDataProvider(const AwsLevel2ChunksDataProvider&) = delete;
   AwsLevel2ChunksDataProvider&
   operator=(const AwsLevel2ChunksDataProvider&) = delete;

   AwsLevel2ChunksDataProvider(AwsLevel2ChunksDataProvider&&) noexcept;
   AwsLevel2ChunksDataProvider&
   operator=(AwsLevel2ChunksDataProvider&&) noexcept;

   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   [[nodiscard]] size_t cache_size() const override;

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
   std::tuple<bool, size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;
   std::pair<size_t, size_t> Refresh() override;

   void                     RequestAvailableProducts() override;
   std::vector<std::string> GetAvailableProducts() override;

   std::optional<float> GetCurrentElevation();

   void SetLevel2DataProvider(
      const std::shared_ptr<AwsLevel2DataProvider>& provider);

   /**
    * @brief Shuts down the provider and stops any in-progress network requests.
    */
   void Shutdown() noexcept override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
