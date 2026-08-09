#include <scwx/config/ondas_config_loader.hpp>
#include <scwx/common/application_state.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>
#include <cpr/cpr.h>
#include <fmt/format.h>

#include <memory>
#include <mutex>
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

// Per-URL fetch mutexes coalesce concurrent first-time lookups for the same key
static std::unordered_map<std::string, std::shared_ptr<std::mutex>>
                  fetchMutexes_;
static std::mutex fetchMutexesMutex_;

static OndasConfigLoader::Result FetchConfig(const std::string& baseUri,
                                             const std::string& configFile)
{
   OndasConfigLoader::Result result {};

   // Fetch config from server
   const ::cpr::Response response =
      ::cpr::Get(::cpr::Url {fmt::format("{0}/{1}", baseUri, configFile)},
                 network::cpr::GetHeader(),
                 network::cpr::GetDefaultTimeout(),
                 network::cpr::GetDefaultConnectTimeout(),
                 network::cpr::GetDefaultLowSpeed(),
                 network::cpr::GetDefaultProgressCallback(
                    common::ApplicationState::IsRunning()));

   if (cpr::status::is_success(response.status_code))
   {
      auto config = std::make_shared<OndasConfig>();
      auto is     = std::istringstream(response.text);
      config->Parse(is);

      result.status = OndasConfigLoader::Status::Loaded;
      result.config = config;
   }
   else if (cpr::status::is_client_error(response.status_code))
   {
      logger_->debug("Config file not found: {0}/{1}", baseUri, configFile);
      result.status = OndasConfigLoader::Status::NotFound;
   }
   else
   {
      logger_->warn("Failed to fetch config file: {0}/{1} ({2})",
                    baseUri,
                    configFile,
                    response.status_code);
      result.status = OndasConfigLoader::Status::Error;
   }

   return result;
}

OndasConfigLoader::Result OndasConfigLoader::Fetch(const std::string& baseUri)
{
   auto result = FetchConfig(baseUri, "config.cfg");

   if (result.config == nullptr)
   {
      const auto oldStatus = result.status;
      result               = FetchConfig(baseUri, "grlevel2.cfg");

      // If neither config file was loaded, set the proper error status
      if (result.status != Status::Loaded && oldStatus == Status::Error)
      {
         result.status = oldStatus;
      }
   }

   if (result.config == nullptr)
   {
      const auto oldStatus = result.status;
      result               = FetchConfig(baseUri, "grlevel3.cfg");

      // If neither config file was loaded, set the proper error status
      if (result.status != Status::Loaded && oldStatus == Status::Error)
      {
         result.status = oldStatus;
      }
   }

   return result;
}

OndasConfigLoader::Result OndasConfigLoader::Get(const std::string& baseUri)
{
   const auto parsed = boost::urls::parse_uri(baseUri);

   // If the URL is invalid, return not found (error would trigger retry)
   if (!parsed.has_value())
   {
      return {.status = Status::NotFound, .config = nullptr};
   }

   boost::urls::url url {*parsed};
   url.normalize();
   std::string key = url.buffer();

   // Remove trailing slash
   if (!key.empty() && key.back() == '/')
   {
      key.pop_back();
   }

   // Fast path: shared lock
   {
      const std::shared_lock lock(cacheMutex_);
      if (auto it = cache_.find(key); it != cache_.end())
      {
         const Status status = it->second ? Status::Loaded : Status::NotFound;
         return {.status = status, .config = it->second};
      }
   }

   // Slow path: coalesce fetches per key; different keys fetch concurrently.
   std::shared_ptr<std::mutex> fetchMutex;
   {
      const std::lock_guard lock(fetchMutexesMutex_);
      fetchMutex =
         fetchMutexes_.try_emplace(key, std::make_shared<std::mutex>())
            .first->second;
   }

   const std::lock_guard fetchLock(*fetchMutex);

   {
      const std::shared_lock lock(cacheMutex_);
      if (auto it = cache_.find(key); it != cache_.end())
      {
         const Status status = it->second ? Status::Loaded : Status::NotFound;
         return {.status = status, .config = it->second};
      }
   }

   // Do not cache error status; transient failures can retry on next call.
   // Caching of null when not found is intentional.
   auto result = OndasConfigLoader::Fetch(key);
   if (result.status != Status::Error)
   {
      const std::unique_lock lock(cacheMutex_);
      cache_.insert_or_assign(key, result.config);
   }

   return result;
}

} // namespace scwx::config
