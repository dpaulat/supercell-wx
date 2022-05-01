#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace scwx
{
namespace provider
{

class AwsLevel2DataProviderImpl;

class AwsLevel2DataProvider
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

   void ListObjects(std::chrono::system_clock::time_point date);
   std::shared_ptr<wsr88d::Ar2vFile> LoadObjectByKey(const std::string& key);
   void                              Refresh();

   static std::chrono::system_clock::time_point
   GetTimePointFromKey(const std::string& key);

private:
   std::unique_ptr<AwsLevel2DataProviderImpl> p;
};

} // namespace provider
} // namespace scwx
