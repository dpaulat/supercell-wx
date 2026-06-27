#include <scwx/config/ondas_config_loader.hpp>
#include <scwx/common/application_state.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <boost/url/url.hpp>
#include <cpr/cpr.h>
#include <fmt/format.h>

#include <shared_mutex>
#include <sstream>
#include <unordered_map>

namespace scwx::config
{

static const std::string logPrefix_ = "scwx::config::ondas_config_loader";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Process-wide cache
static std::unordered_map<std::string, std::shared_ptr<const OndasConfig>>
                         cache_;
static std::shared_mutex cacheMutex_;

static std::shared_ptr<const OndasConfig>
FetchConfig(const std::string& baseUri, const std::string& configFile)
{
   std::shared_ptr<OndasConfig> config {};

   // Fetch config from server
   ::cpr::Response response =
      ::cpr::Get(::cpr::Url {fmt::format("{0}/{1}", baseUri, configFile)},
                 network::cpr::GetHeader(),
                 network::cpr::GetDefaultTimeout(),
                 network::cpr::GetDefaultConnectTimeout(),
                 network::cpr::GetDefaultLowSpeed(),
                 network::cpr::GetDefaultProgressCallback(
                    common::ApplicationState::IsRunning()));

   if (response.status_code == ::cpr::status::HTTP_OK)
   {
      config  = std::make_shared<OndasConfig>();
      auto is = std::istringstream(response.text);
      config->Parse(is);
   }
   else if (response.status_code == ::cpr::status::HTTP_NOT_FOUND)
   {
      logger_->debug("Config file not found: {0}/{1}", baseUri, configFile);
   }
   else
   {
      logger_->warn(
         "Failed to fetch config file: {0}/{1}", baseUri, configFile);
   }

   return config;
}

std::shared_ptr<const OndasConfig>
OndasConfigLoader::Fetch(const std::string& baseUri)
{
   auto cfg = FetchConfig(baseUri, "config.cfg");
   if (cfg == nullptr)
   {
      cfg = FetchConfig(baseUri, "grlevel2.cfg");
   }
   return cfg;
}

std::shared_ptr<const OndasConfig>
OndasConfigLoader::Get(const std::string& baseUri)
{
   const std::string key = boost::urls::url(baseUri).normalize().buffer();

   // Fast path: shared lock, promote weak_ptr
   {
      std::shared_lock lock(cacheMutex_);
      if (auto it = cache_.find(key); it != cache_.end())
      {
         return it->second;
      }
   }

   // Slow path: unique lock, double-check, fetch once
   std::unique_lock lock(cacheMutex_);
   if (auto it = cache_.find(key); it != cache_.end())
   {
      return it->second;
   }

   // Do not cache nullptr; transient failures can retry on next call
   auto cfg = OndasConfigLoader::Fetch(key);
   if (cfg)
   {
      cache_.insert_or_assign(key, cfg);
   }

   return cfg;
}

} // namespace scwx::config
