#pragma once

#include <scwx/types/nws_types.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <boost/outcome/result.hpp>

namespace scwx::provider
{

/**
 * @brief NWS API Provider
 */
class NwsApiProvider
{
public:
   explicit NwsApiProvider();
   ~NwsApiProvider();

   NwsApiProvider(const NwsApiProvider&)            = delete;
   NwsApiProvider& operator=(const NwsApiProvider&) = delete;

   NwsApiProvider(NwsApiProvider&&) noexcept;
   NwsApiProvider& operator=(NwsApiProvider&&) noexcept;

   /**
    * @brief Returns a list of radar stations
    *
    * @param [in] stationType Limit results to a specific station type or types
    * @param [in] reportingHost Show RDA and latency info from specific
    * reporting host
    * @param [in] host Show latency info from specific LDM host
    */
   boost::outcome_v2::result<std::vector<types::nws::ObservationStation>>
   GetRadarStations(
      const std::vector<std::string>& stationType = std::vector<std::string> {},
      std::optional<std::string_view> reportingHost = std::nullopt,
      std::optional<std::string_view> host          = std::nullopt);

   /**
    * @brief Shuts down the provider and stops any in-progress network requests.
    */
   void Shutdown() noexcept;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
