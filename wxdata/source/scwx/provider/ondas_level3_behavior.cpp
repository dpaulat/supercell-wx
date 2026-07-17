#include <algorithm>
#include <scwx/provider/ondas_level3_behavior.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/types/ondas_types.hpp>
#include <scwx/util/hash.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <shared_mutex>
#include <unordered_map>

#include <cpr/cpr.h>
#include <fmt/format.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::ondas_level3_behavior";
static const auto        logger_    = util::Logger::Create(logPrefix_);

namespace
{

class OndasLevel3SiteData
{
public:
   OndasLevel3SiteData(std::string                                baseUri,
                       std::string                                radarSite,
                       std::shared_ptr<const config::OndasConfig> config) :
       baseUri_ {std::move(baseUri)},
       radarSite_ {std::move(radarSite)},
       config_ {std::move(config)}
   {
   }
   ~OndasLevel3SiteData() = default;

   OndasLevel3SiteData(const OndasLevel3SiteData&)                = delete;
   OndasLevel3SiteData& operator=(const OndasLevel3SiteData&)     = delete;
   OndasLevel3SiteData(OndasLevel3SiteData&&) noexcept            = delete;
   OndasLevel3SiteData& operator=(OndasLevel3SiteData&&) noexcept = delete;

   void                     ListProducts(std::atomic<bool>& running);
   std::vector<std::string> GetAvailableProducts();

   static std::shared_ptr<OndasLevel3SiteData>
   Instance(const std::string&                                baseUri,
            const std::string&                                radarSite,
            const std::shared_ptr<const config::OndasConfig>& config);

   const std::string                                baseUri_;
   const std::string                                radarSite_;
   const std::shared_ptr<const config::OndasConfig> config_;

   std::mutex               listProductsMutex_;
   std::shared_mutex        productsMutex_;
   std::vector<std::string> availableProducts_ {};
   bool                     productsReady_ {false};
};

} // namespace

class OndasLevel3Behavior::Impl
{
public:
   explicit Impl(std::string                                baseUri,
                 std::string                                radarSite,
                 std::string                                product,
                 std::shared_ptr<const config::OndasConfig> config) :
       baseUri_ {std::move(baseUri)},
       radarSite_ {std::move(radarSite)},
       product_ {std::move(product)},
       config_ {std::move(config)}
   {
      productPath_ = config_->ApplySiteSubstitution(radarSite_, product_);
      listingUrl_  = fmt::format(
         "{0}/{1}/{2}", baseUri_, productPath_, config_->list_file());
   }
   ~Impl() = default;

   Impl(const Impl&)                = delete;
   Impl& operator=(const Impl&)     = delete;
   Impl(Impl&&) noexcept            = delete;
   Impl& operator=(Impl&&) noexcept = delete;

   const std::string                                baseUri_;
   const std::string                                radarSite_;
   const std::string                                product_;
   const std::shared_ptr<const config::OndasConfig> config_;

   std::string productPath_;
   std::string listingUrl_;

   std::atomic<bool> running_ {true};
};

OndasLevel3Behavior::OndasLevel3Behavior(
   const std::string&                                baseUri,
   const std::string&                                radarSite,
   const std::string&                                product,
   const std::shared_ptr<const config::OndasConfig>& config) :
    IHttpLevel3ServerBehavior(),
    p {std::make_unique<Impl>(baseUri, radarSite, product, config)}
{
}

OndasLevel3Behavior::~OndasLevel3Behavior() = default;

void OndasLevel3Behavior::Shutdown() noexcept
{
   p->running_ = false;
}

std::pair<bool, std::vector<std::string>>
OndasLevel3Behavior::ListObjects(std::chrono::system_clock::time_point date)
{
   (void) date; // Not needed since ONDAS dir.list contains all dates

   logger_->debug("ListObjects: {}", p->listingUrl_);

   // Download dir.list
   const auto response =
      network::cpr::DownloadToString(p->listingUrl_, p->running_);
   const std::string& content    = response.first;
   const long         statusCode = response.second;

   // Treat 2xx and 4xx status codes as success, and 5xx (server errors) as
   // error.
   const bool success = statusCode >= cpr::status::SUCCESS_CODE_OFFSET &&
                        statusCode < cpr::status::SERVER_ERROR_CODE_OFFSET;

   if (content.empty())
   {
      return {success, {}};
   }

   // Parse ONDAS format
   const auto               records = types::ondas::ParseOndasDirList(content);
   std::vector<std::string> keys;

   std::ranges::transform(records,
                          std::back_inserter(keys),
                          [](const auto& record) { return record.filename_; });

   return {true, std::move(keys)};
}

std::string OndasLevel3Behavior::GetFileUrl(const std::string& key) const
{
   // ONDAS file URL format is:
   // {baseUri}/{productPath}/{key}
   // Example: https://ondas.example.com/iln/N0R/20260131_1830.raw
   return fmt::format("{0}/{1}/{2}", p->baseUri_, p->productPath_, key);
}

std::chrono::system_clock::time_point
OndasLevel3Behavior::GetTimePointByKey(const std::string& key) const
{
   return GetTimePointFromKey(key);
}

std::chrono::system_clock::time_point
OndasLevel3Behavior::GetTimePointFromKey(const std::string& key)
{
   return config::OndasConfig::GetTimePointFromFilename(key);
}

void OndasLevel3Behavior::RequestAvailableProducts()
{
   OndasLevel3SiteData::Instance(p->baseUri_, p->radarSite_, p->config_)
      ->ListProducts(p->running_);
}

void OndasLevel3SiteData::ListProducts(std::atomic<bool>& running)
{
   const std::unique_lock listProductsLock {listProductsMutex_};
   std::shared_lock       readLock {productsMutex_};

   // Only list products once per site
   if (productsReady_)
   {
      return;
   }

   readLock.unlock();

   logger_->debug("ListProducts: {} {}", baseUri_, radarSite_);

   std::vector<
      std::pair<std::string, cpr::AsyncWrapper<std::pair<bool, bool>, false>>>
      asyncCallbacks {};

   std::vector<std::string> products;
   bool                     error = false;

   for (const auto& product : config_->products())
   {
      const std::string productPath =
         config_->ApplySiteSubstitution(radarSite_, product);
      const std::string listingUrl = fmt::format(
         "{0}/{1}/{2}", baseUri_, productPath, config_->list_file());

      // Query to see if product is available for this radar site
      asyncCallbacks.emplace_back(
         product,
         cpr::HeadCallback(
            [](cpr::Response headResponse) -> std::pair<bool, bool>
            {
               bool productExists = false;
               bool error         = false;

               if (headResponse.status_code == cpr::status::HTTP_OK)
               {
                  productExists = true;
               }
               else if (headResponse.status_code != cpr::status::HTTP_NOT_FOUND)
               {
                  // Error checking if product exists
                  logger_->warn("Error checking if product exists: {} ({})",
                                (headResponse.status_code == 0) ?
                                   headResponse.error.message :
                                   headResponse.status_line,
                                headResponse.status_code);
                  error = true;
               }

               return {productExists, error};
            },
            cpr::Url {listingUrl},
            network::cpr::GetHeader(),
            network::cpr::GetDefaultTimeout(),
            network::cpr::GetDefaultConnectTimeout(),
            network::cpr::GetDefaultLowSpeed(),
            network::cpr::GetDefaultProgressCallback(running)));
   }

   for (auto& asyncCallback : asyncCallbacks)
   {
      auto [productExists, productError] = asyncCallback.second.get();
      if (productExists)
      {
         // If the directory listing exists, then the product is available
         products.emplace_back(asyncCallback.first);
      }
      else if (productError)
      {
         error = true;
      }
   }

   if (!error)
   {
      // If no errors, then update the available products
      const std::unique_lock writeLock {productsMutex_};

      // Move the products into the available products vector
      availableProducts_.swap(products);
      productsReady_ = true;
   }
}

std::vector<std::string> OndasLevel3Behavior::GetAvailableProducts() const
{
   return OndasLevel3SiteData::Instance(p->baseUri_, p->radarSite_, p->config_)
      ->GetAvailableProducts();
}

std::vector<std::string> OndasLevel3SiteData::GetAvailableProducts()
{
   const std::shared_lock lock {productsMutex_};
   return availableProducts_;
}

bool OndasLevel3Behavior::date_archive_available() const
{
   // Not supported
   return false;
}

std::shared_ptr<OndasLevel3SiteData> OndasLevel3SiteData::Instance(
   const std::string&                                baseUri,
   const std::string&                                radarSite,
   const std::shared_ptr<const config::OndasConfig>& config)
{
   static std::unordered_map<std::pair<std::string, std::string>,
                             std::shared_ptr<OndasLevel3SiteData>,
                             util::hash<std::pair<std::string, std::string>>>
                            instanceMap_;
   static std::shared_mutex instanceMutex_;

   std::shared_ptr<OndasLevel3SiteData> instance = nullptr;

   {
      const std::unique_lock lock {instanceMutex_};

      // Look up instance shared pointer
      auto it = instanceMap_.find(std::make_pair(baseUri, radarSite));
      if (it != instanceMap_.end())
      {
         instance = it->second;
      }

      // If no active instance was found, create a new one
      if (instance == nullptr)
      {
         instance =
            std::make_shared<OndasLevel3SiteData>(baseUri, radarSite, config);
         instanceMap_.insert_or_assign(std::make_pair(baseUri, radarSite),
                                       instance);
      }
   }

   return instance;
}

} // namespace scwx::provider
