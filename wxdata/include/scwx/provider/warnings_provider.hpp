#pragma once

#include <scwx/awips/text_product_file.hpp>
#include <scwx/network/cpr.hpp>

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

   // `progressCallback`, when supplied, reports incremental byte
   // progress for each hour's actual warnings-file GET request (not the
   // preceding per-hour HEAD check, which is metadata-only and not worth
   // reporting) -- see network::cpr::DownloadProgressCallback's own doc
   // for why this is a callback and not a direct dependency on any
   // status-reporting singleton (this class lives in wxdata, which does
   // not link Qt/scwx-qt).
   std::vector<std::shared_ptr<awips::TextProductFile>> LoadUpdatedFiles(
      std::chrono::sys_time<std::chrono::hours>     newerThan        = {},
      const network::cpr::DownloadProgressCallback& progressCallback = nullptr);

   /**
    * @brief Shuts down the provider and stops any in-progress network requests.
    */
   void Shutdown() noexcept;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
