#pragma once

#include <scwx/awips/text_product_file.hpp>

#include <chrono>
#include <optional>

namespace scwx::provider
{

/**
 * @brief Warnings Provider
 */
class WarningsProvider
{
public:
   explicit WarningsProvider(const std::string& baseUrl);
   ~WarningsProvider();

   WarningsProvider(const WarningsProvider&)            = delete;
   WarningsProvider& operator=(const WarningsProvider&) = delete;

   WarningsProvider(WarningsProvider&&) noexcept;
   WarningsProvider& operator=(WarningsProvider&&) noexcept;

   std::vector<std::shared_ptr<awips::TextProductFile>> LoadUpdatedFiles(
      std::chrono::sys_time<std::chrono::hours>                startTime = {},
      std::optional<std::chrono::sys_time<std::chrono::hours>> endBefore =
         std::nullopt);

   /**
    * @brief Shuts down the provider and stops any in-progress network requests.
    */
   void Shutdown() noexcept;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
