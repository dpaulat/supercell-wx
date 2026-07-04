#pragma once

#include <scwx/provider/http_level3_server_behavior.hpp>

#include <memory>

namespace scwx::provider
{

class NwsLevel3Behavior : public IHttpLevel3ServerBehavior
{
public:
   explicit NwsLevel3Behavior(const std::string& baseUri,
                              const std::string& radarSite,
                              const std::string& product);
   ~NwsLevel3Behavior() override;

   NwsLevel3Behavior(const NwsLevel3Behavior&)            = delete;
   NwsLevel3Behavior& operator=(const NwsLevel3Behavior&) = delete;

   NwsLevel3Behavior(NwsLevel3Behavior&&) noexcept            = default;
   NwsLevel3Behavior& operator=(NwsLevel3Behavior&&) noexcept = default;

   void Shutdown() noexcept override;

   std::vector<std::string>
   ListObjects(std::chrono::system_clock::time_point date) override;

   [[nodiscard]] std::string GetFileUrl(const std::string& key) const override;
   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   void                                   RequestAvailableProducts() override;
   [[nodiscard]] std::vector<std::string> GetAvailableProducts() const override;

   [[nodiscard]] bool date_archive_available() const override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
