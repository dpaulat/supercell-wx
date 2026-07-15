#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace scwx::provider
{

class IHttpLevel3ServerBehavior
{
public:
   IHttpLevel3ServerBehavior()          = default;
   virtual ~IHttpLevel3ServerBehavior() = default;

   IHttpLevel3ServerBehavior(const IHttpLevel3ServerBehavior&) = delete;
   IHttpLevel3ServerBehavior&
   operator=(const IHttpLevel3ServerBehavior&) = delete;

   IHttpLevel3ServerBehavior(IHttpLevel3ServerBehavior&&)            = delete;
   IHttpLevel3ServerBehavior& operator=(IHttpLevel3ServerBehavior&&) = delete;

   virtual void Shutdown() noexcept = 0;

   virtual std::pair<bool, std::vector<std::string>>
   ListObjects(std::chrono::system_clock::time_point date) = 0;

   [[nodiscard]] virtual std::string
   GetFileUrl(const std::string& key) const = 0;
   [[nodiscard]] virtual std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const = 0;

   virtual void RequestAvailableProducts() = 0;
   [[nodiscard]] virtual std::vector<std::string>
   GetAvailableProducts() const = 0;

   [[nodiscard]] virtual bool date_archive_available() const = 0;
};

} // namespace scwx::provider
