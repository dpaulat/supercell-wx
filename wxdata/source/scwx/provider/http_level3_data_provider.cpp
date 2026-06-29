#include <scwx/provider/http_level3_data_provider.hpp>
#include <scwx/provider/http_level3_server_behavior.hpp>
#include <scwx/provider/nws_level3_behavior.hpp>
#include <scwx/provider/ondas_level3_behavior.hpp>
#include <scwx/config/ondas_config_loader.hpp>
#include <scwx/util/logger.hpp>

namespace scwx::provider
{

static const std::string logPrefix_ =
   "scwx::provider::http_level3_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

class HttpLevel3DataProvider::Impl
{
public:
   explicit Impl(HttpLevel3DataProvider* self,
                 std::string             radarSite,
                 std::string             product,
                 std::string             baseUri) :
       self_ {self},
       radarSite_ {std::move(radarSite)},
       product_ {std::move(product)},
       baseUri_ {std::move(baseUri)}
   {
   }
   ~Impl() = default;

   Impl(const Impl&)                = delete;
   Impl& operator=(const Impl&)     = delete;
   Impl(Impl&&) noexcept            = delete;
   Impl& operator=(Impl&&) noexcept = delete;

   void DetectServerBehavior();

   HttpLevel3DataProvider* self_;
   std::string             radarSite_;
   std::string             product_;
   std::string             baseUri_;

   std::unique_ptr<IHttpLevel3ServerBehavior> serverBehavior_ {};

   std::mutex listObjectsMutex_ {};
};

HttpLevel3DataProvider::HttpLevel3DataProvider(const std::string& radarSite,
                                               const std::string& product,
                                               const std::string& baseUri) :
    HttpNexradDataProvider(radarSite, baseUri),
    p(std::make_unique<Impl>(this, radarSite, product, baseUri))
{
}

HttpLevel3DataProvider::~HttpLevel3DataProvider() = default;

HttpLevel3DataProvider::HttpLevel3DataProvider(
   HttpLevel3DataProvider&&) noexcept = default;
HttpLevel3DataProvider&
HttpLevel3DataProvider::operator=(HttpLevel3DataProvider&&) noexcept = default;

void HttpLevel3DataProvider::Impl::DetectServerBehavior()
{
   // If the server behavior has already been detected, return
   if (serverBehavior_)
   {
      return;
   }

   // Try to load the ONDAS config
   const auto result = config::OndasConfigLoader::Get(baseUri_);

   if (result.status == config::OndasConfigLoader::Status::Loaded)
   {
      // If the ONDAS config is loaded, use the ONDAS level 3 behavior
      serverBehavior_ = std::make_unique<OndasLevel3Behavior>(
         baseUri_, radarSite_, product_, result.config);
   }
   else if (result.status == config::OndasConfigLoader::Status::NotFound)
   {
      // If the ONDAS config is not found, use the NWS level 3 behavior
      serverBehavior_ = std::make_unique<NwsLevel3Behavior>();
   }
   else
   {
      // If there is an error loading the ONDAS config, don't use any server
      // behavior
      logger_->warn("Failed to detect server behavior for {0}", baseUri_);
   }
}

void HttpLevel3DataProvider::Shutdown() noexcept
{
   if (p->serverBehavior_)
   {
      p->serverBehavior_->Shutdown();
   }
}

std::tuple<bool, std::size_t, std::size_t>
HttpLevel3DataProvider::ListObjects(std::chrono::system_clock::time_point date)
{
   p->DetectServerBehavior();
   if (!p->serverBehavior_)
   {
      const std::unique_lock lock {p->listObjectsMutex_};
      ResetCacheStart();
      ResetCacheFinish();
      return {false, 0, 0};
   }

   const auto objects = p->serverBehavior_->ListObjects(date);
   if (objects.empty())
   {
      const std::unique_lock lock {p->listObjectsMutex_};
      ResetCacheStart();
      ResetCacheFinish();
      return {false, 0, 0};
   }

   std::size_t newObjects   = 0;
   std::size_t totalObjects = 0;

   const std::unique_lock lock {p->listObjectsMutex_};

   ResetCacheStart();

   for (const auto& object : objects)
   {
      const auto time = p->serverBehavior_->GetTimePointByKey(object);
      if (time == std::chrono::system_clock::time_point {})
      {
         continue; // Invalid timestamp
      }

      // Add to cache (key is the filename)
      bool inserted = AddToCache(time, object, time);
      if (inserted)
      {
         newObjects++;
      }
      totalObjects++;
   }

   ResetCacheFinish();

   return {true, newObjects, totalObjects};
}

std::string HttpLevel3DataProvider::GetFileUrl(const std::string& key)
{
   // Don't call DetectServerBehavior() here to avoid blocking the main thread.
   // Behavior should have already been detected.
   if (!p->serverBehavior_)
   {
      return {};
   }
   return p->serverBehavior_->GetFileUrl(key);
}

std::chrono::system_clock::time_point
HttpLevel3DataProvider::GetTimePointByKey(const std::string& key) const
{
   // Don't call DetectServerBehavior() here to avoid blocking the main thread.
   // Behavior should have already been detected.
   if (!p->serverBehavior_)
   {
      return {};
   }
   return p->serverBehavior_->GetTimePointByKey(key);
}

void HttpLevel3DataProvider::RequestAvailableProducts()
{
   p->DetectServerBehavior();
   if (!p->serverBehavior_)
   {
      return;
   }
   p->serverBehavior_->RequestAvailableProducts();
}

std::vector<std::string> HttpLevel3DataProvider::GetAvailableProducts()
{
   // Don't call DetectServerBehavior() here to avoid blocking the main thread
   if (!p->serverBehavior_)
   {
      return {};
   }
   return p->serverBehavior_->GetAvailableProducts();
}

} // namespace scwx::provider
