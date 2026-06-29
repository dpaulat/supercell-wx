#include <algorithm>
#include <scwx/provider/ondas_level3_behavior.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/types/ondas_types.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>

#include <fmt/format.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::ondas_level3_behavior";
static const auto        logger_    = util::Logger::Create(logPrefix_);

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

   std::vector<std::string> availableProducts_ {};
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

std::vector<std::string>
OndasLevel3Behavior::ListObjects(std::chrono::system_clock::time_point date)
{
   (void) date; // Not needed since ONDAS dir.list contains all dates

   logger_->debug("ListObjects: {}", p->listingUrl_);

   // Download dir.list
   const std::string content =
      network::cpr::DownloadToString(p->listingUrl_, p->running_);
   if (content.empty())
   {
      return {};
   }

   // Parse ONDAS format
   const auto               records = types::ondas::ParseOndasDirList(content);
   std::vector<std::string> keys;

   std::ranges::transform(records,
                          std::back_inserter(keys),
                          [](const auto& record) { return record.filename_; });

   return keys;
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
   for (const auto& product : p->config_->products())
   {
      // TODO: Query to see if product is available for this radar site. Also,
      // should this move somewhere else? This should be called once per-site,
      // not for each product.
      p->availableProducts_.emplace_back(product);
   }
}

std::vector<std::string> OndasLevel3Behavior::GetAvailableProducts() const
{
   return p->availableProducts_;
}

bool OndasLevel3Behavior::date_archive_available() const
{
   // Not supported
   return false;
}

} // namespace scwx::provider
