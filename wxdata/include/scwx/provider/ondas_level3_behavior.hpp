#pragma once

#include <scwx/config/ondas_config.hpp>
#include <scwx/provider/http_level3_server_behavior.hpp>

#include <memory>

namespace scwx::provider
{

class OndasLevel3Behavior : public IHttpLevel3ServerBehavior
{
public:
   explicit OndasLevel3Behavior(
      const std::string&                                baseUri,
      const std::string&                                radarSite,
      const std::string&                                product,
      const std::shared_ptr<const config::OndasConfig>& config);
   ~OndasLevel3Behavior() override;

   OndasLevel3Behavior(const OndasLevel3Behavior&)            = delete;
   OndasLevel3Behavior& operator=(const OndasLevel3Behavior&) = delete;

   OndasLevel3Behavior(OndasLevel3Behavior&&)            = delete;
   OndasLevel3Behavior& operator=(OndasLevel3Behavior&&) = delete;

   void Shutdown() noexcept override;

   std::vector<std::string>
   ListObjects(std::chrono::system_clock::time_point date) override;

   [[nodiscard]] std::string GetFileUrl(const std::string& key) const override;
   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   static std::chrono::system_clock::time_point
   GetTimePointFromKey(const std::string& key);

   void                                   RequestAvailableProducts() override;
   [[nodiscard]] std::vector<std::string> GetAvailableProducts() const override;

   [[nodiscard]] bool date_archive_available() const override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
