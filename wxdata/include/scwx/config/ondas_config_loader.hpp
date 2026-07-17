#pragma once

#include <scwx/config/ondas_config.hpp>

namespace scwx::config
{

class OndasConfigLoader
{
public:
   explicit OndasConfigLoader() = delete;
   ~OndasConfigLoader()         = delete;

   OndasConfigLoader(const OndasConfigLoader&)            = delete;
   OndasConfigLoader& operator=(const OndasConfigLoader&) = delete;

   OndasConfigLoader(OndasConfigLoader&&) noexcept            = delete;
   OndasConfigLoader& operator=(OndasConfigLoader&&) noexcept = delete;

   /**
    * @brief Status of the ONDAS config loader.
    */
   enum class Status : std::uint8_t
   {
      Loaded,
      NotFound,
      Error
   };

   /**
    * @brief Result of the ONDAS config loader.
    */
   struct Result
   {
      Status                             status {Status::Error};
      std::shared_ptr<const OndasConfig> config {};
   };

   /**
    * @brief Fetch ONDAS config from a server (always hits network).
    *
    * Tries {baseUri}/config.cfg, then {baseUri}/grlevel2.cfg.
    * @param baseUri Server root URL (normalized internally).
    * @return Result containing the status and parsed config.
    */
   static Result Fetch(const std::string& baseUri);

   /**
    * @brief Get cached ONDAS config for a server, fetching on first miss.
    *
    * Thread-safe. One successful fetch per normalized baseUri for process
    * lifetime. Concurrent first callers for the same baseUri coalesce to a
    * single fetch.
    * @param baseUri Server root URL.
    * @return Result containing the status and cached config.
    */
   static Result Get(const std::string& baseUri);
};

} // namespace scwx::config
