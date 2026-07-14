#include <scwx/provider/nws_level3_behavior.hpp>
#include <scwx/common/application_state.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/network/dir_list.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <deque>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/url/url.hpp>
#include <cpr/cpr.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <re2/re2.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::nws_level3_behavior";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Map of product code to directory name
static const boost::unordered_flat_map<std::string, std::string>
   kProductDirectoryMap_ {
      {"DHR", "DS.32dhr"}, {"NVW", "DS.48vwp"}, {"N0S", "DS.56rm0"},
      {"N1S", "DS.56rm1"}, {"N2S", "DS.56rm2"}, {"N3S", "DS.56rm3"},
      {"NVL", "DS.57vil"}, {"NST", "DS.58sti"}, {"NTV", "DS.61tvs"},
      {"NML", "DS.66lrm"}, {"NLA", "DS.67apr"}, {"RCM", "DS.74rcm"},
      {"FTM", "DS.75ftm"}, {"N1P", "DS.78ohp"}, {"N3P", "DS.79thp"},
      {"NTP", "DS.80stp"}, {"DPA", "DS.81dpr"}, {"SPD", "DS.82spd"},
      {"NHL", "DS.90lrm"}, {"N0F", "DS.113f0"}, {"N1F", "DS.113f1"},
      {"N2F", "DS.113f2"}, {"N3F", "DS.113f3"}, {"NAF", "DS.113fa"},
      {"NBF", "DS.113fb"}, {"NXF", "DS.113fx"}, {"NYF", "DS.113fy"},
      {"NZF", "DS.113fz"}, {"DVL", "DS.134il"}, {"EET", "DS.135et"},
      {"DSP", "DS.138dp"}, {"NMD", "DS.141md"}, {"RSL", "DS.152rs"},
      {"N0X", "DS.159x0"}, {"N1X", "DS.159x1"}, {"N2X", "DS.159x2"},
      {"N3X", "DS.159x3"}, {"NAX", "DS.159xa"}, {"NBX", "DS.159xb"},
      {"NXX", "DS.159xx"}, {"NYX", "DS.159xy"}, {"NZX", "DS.159xz"},
      {"N0C", "DS.161c0"}, {"N1C", "DS.161c1"}, {"N2C", "DS.161c2"},
      {"N3C", "DS.161c3"}, {"NAC", "DS.161ca"}, {"NBC", "DS.161cb"},
      {"NXC", "DS.161cx"}, {"NYC", "DS.161cy"}, {"NZC", "DS.161cz"},
      {"N0K", "DS.163k0"}, {"N1K", "DS.163k1"}, {"N2K", "DS.163k2"},
      {"N3K", "DS.163k3"}, {"NAK", "DS.163ka"}, {"NBK", "DS.163kb"},
      {"NXK", "DS.163kx"}, {"NYK", "DS.163ky"}, {"NZK", "DS.163kz"},
      {"N0H", "DS.165h0"}, {"N1H", "DS.165h1"}, {"N2H", "DS.165h2"},
      {"N3H", "DS.165h3"}, {"NAH", "DS.165ha"}, {"NBH", "DS.165hb"},
      {"NXH", "DS.165hx"}, {"NYH", "DS.165hy"}, {"NZH", "DS.165hz"},
      {"N0M", "DS.166m0"}, {"N1M", "DS.166m1"}, {"N2M", "DS.166m2"},
      {"N3M", "DS.166m3"}, {"NAM", "DS.166ma"}, {"NBM", "DS.166mb"},
      {"NXM", "DS.166mx"}, {"NYM", "DS.166my"}, {"NZM", "DS.166mz"},
      {"OHA", "DS.169oh"}, {"DAA", "DS.170aa"}, {"DTA", "DS.172dt"},
      {"DU3", "DS.173u1"}, {"DU6", "DS.173u3"}, {"DOD", "DS.174od"},
      {"DSD", "DS.175sd"}, {"DPR", "DS.176pr"}, {"HHC", "DS.177hh"},
      {"TZ0", "DS.180z0"}, {"TZ1", "DS.180z1"}, {"TZ2", "DS.180z2"},
      {"TV0", "DS.182v0"}, {"TV1", "DS.182v1"}, {"TV2", "DS.182v2"},
      {"TZL", "DS.186zl"}, {"NRR", "DS.197rr"}, {"GSM", "DS.p2gsm"},
      {"NSW", "DS.p30sw"}, {"NCR", "DS.p37cr"}, {"NCZ", "DS.p38cr"},
      {"NET", "DS.p41et"}, {"NHI", "DS.p59hi"}, {"NSS", "DS.p62ss"},
      {"N0Q", "DS.p94r0"}, {"N1Q", "DS.p94r1"}, {"N2Q", "DS.p94r2"},
      {"N3Q", "DS.p94r3"}, {"NAQ", "DS.p94ra"}, {"NBQ", "DS.p94rb"},
      {"NXQ", "DS.p94rx"}, {"NYQ", "DS.p94ry"}, {"NZQ", "DS.p94rz"},
      {"N0U", "DS.p99v0"}, {"N1U", "DS.p99v1"}, {"N2U", "DS.p99v2"},
      {"N3U", "DS.p99v3"}, {"NAU", "DS.p99va"}, {"NBU", "DS.p99vb"},
      {"NXU", "DS.p99vx"}, {"NYU", "DS.p99vy"}, {"NZU", "DS.p99vz"}};

namespace
{

class NwsLevel3SiteData
{
public:
   NwsLevel3SiteData(std::string baseUri) : baseUri_ {std::move(baseUri)} {}
   ~NwsLevel3SiteData() = default;

   NwsLevel3SiteData(const NwsLevel3SiteData&)                = delete;
   NwsLevel3SiteData& operator=(const NwsLevel3SiteData&)     = delete;
   NwsLevel3SiteData(NwsLevel3SiteData&&) noexcept            = delete;
   NwsLevel3SiteData& operator=(NwsLevel3SiteData&&) noexcept = delete;

   void                     ListProducts();
   std::vector<std::string> GetAvailableProducts(const std::string& radarSite);
   void                     ProcessProductDirectory(const std::string&  product,
                                                    cpr::AsyncResponse& asyncResponse,
                                                    std::atomic<bool>&  error);
   void                     RegisterProduct(const std::string& product,
                                            const std::string& radarSite);

   static std::shared_ptr<NwsLevel3SiteData>
   Instance(const std::string& baseUri);

   const std::string baseUri_;

   std::mutex        listProductsMutex_;
   std::shared_mutex productsMutex_;
   std::unordered_map<std::string, std::vector<std::string>>
                     availableProducts_ {};
   std::atomic<bool> productsReady_ {false};
};

} // namespace

class NwsLevel3Behavior::Impl
{
public:
   explicit Impl(const std::string& baseUri,
                 std::string        radarSite,
                 std::string        product) :
       radarSite_ {std::move(radarSite)}, product_ {std::move(product)}
   {

      // Normalize the base URI
      const auto parsed = boost::urls::parse_uri(baseUri);

      // If the URL is invalid, return
      if (!parsed.has_value())
      {
         logger_->warn("Invalid URL: {}", baseUri);
         return;
      }

      boost::urls::url url {*parsed};
      url.normalize();
      std::string normalizedUri = url.buffer();

      // Remove trailing slash
      if (!normalizedUri.empty() && normalizedUri.back() == '/')
      {
         normalizedUri.pop_back();
      }

      baseUri_ = normalizedUri;

      const auto it = kProductDirectoryMap_.find(product_);
      if (it != kProductDirectoryMap_.end())
      {
         productDirectory_ = it->second;
         productValid_     = true;
      }
      else
      {
         logger_->warn("Product directory not found for product: {}", product_);
      }

      listingUrl_ = fmt::format("{0}/{1}/SI.{2}",
                                baseUri_,
                                productDirectory_,
                                boost::to_lower_copy(radarSite_));
   }
   ~Impl() = default;

   Impl(const Impl&)                = delete;
   Impl& operator=(const Impl&)     = delete;
   Impl(Impl&&) noexcept            = delete;
   Impl& operator=(Impl&&) noexcept = delete;

   std::string       baseUri_ {};
   const std::string radarSite_;
   const std::string product_;

   bool        productValid_ {false};
   std::string productDirectory_ {"?"};
   std::string listingUrl_ {};

   std::shared_mutex objectsMutex_;
   std::unordered_map<std::string, std::chrono::system_clock::time_point>
      objectList_ {};

   std::atomic<bool> running_ {true};
};

NwsLevel3Behavior::NwsLevel3Behavior(const std::string& baseUri,
                                     const std::string& radarSite,
                                     const std::string& product) :
    IHttpLevel3ServerBehavior(),
    p {std::make_unique<Impl>(baseUri, radarSite, product)}
{
}

NwsLevel3Behavior::~NwsLevel3Behavior() = default;

void NwsLevel3Behavior::Shutdown() noexcept
{
   p->running_ = false;
}

std::pair<bool, std::vector<std::string>>
NwsLevel3Behavior::ListObjects(std::chrono::system_clock::time_point date)
{
   (void) date; // Not needed since NWS directory structure contains all dates

   if (!p->productValid_)
   {
      // Skip product listing for invalid products
      return {};
   }

   logger_->debug("ListObjects: {}", p->listingUrl_);

   // Download directory listing
   const auto response =
      network::cpr::DownloadToString(p->listingUrl_, p->running_);
   const std::string& content    = response.first;
   const long         statusCode = response.second;

   // Treat 2xx and 4xx status codes as success, and 5xx (server errors) as
   // error.
   const bool success = statusCode < cpr::status::SERVER_ERROR_CODE_OFFSET;

   if (content.empty())
   {
      return {success, {}};
   }

   std::unordered_map<std::string, std::chrono::system_clock::time_point>
      newObjectList {};

   // Parse directory listing
   const auto records = network::DirList(p->listingUrl_, content);
   for (const auto& record : records)
   {
      if (record.type_ != std::filesystem::file_type::regular)
      {
         continue;
      }

      // Pattern match: sn.nnnn (ignore sn.last)
      static constexpr LazyRE2 re      = {"sn\\.\\d{4}"};
      const bool               isMatch = RE2::FullMatch(record.filename_, *re);
      if (isMatch)
      {
         newObjectList.emplace(record.filename_, record.mtime_);
      }
   }

   // Update object list with latest directory listing
   const std::unique_lock lock {p->objectsMutex_};
   p->objectList_.swap(newObjectList);

   return {true,
           p->objectList_ | ranges::views::keys | ranges::to<std::vector>()};
}

std::string NwsLevel3Behavior::GetFileUrl(const std::string& key) const
{
   // NWS file URL format is:
   // {baseUri}/{productDirectory}/SI.{ssss}/{key}
   // Example: https://example.com/DS.32dhr/SI.klsx/sn.0000
   return fmt::format("{0}/{1}", p->listingUrl_, key);
}

std::chrono::system_clock::time_point
NwsLevel3Behavior::GetTimePointByKey(const std::string& key) const
{
   const std::shared_lock lock {p->objectsMutex_};
   const auto             it = p->objectList_.find(key);
   if (it != p->objectList_.end())
   {
      return it->second;
   }
   return {};
}

void NwsLevel3Behavior::RequestAvailableProducts()
{
   // Request available products from the NWS Level 3 site data
   const auto siteData = NwsLevel3SiteData::Instance(p->baseUri_);
   if (siteData)
   {
      siteData->ListProducts();
   }
}

void NwsLevel3SiteData::ListProducts()
{
   const std::unique_lock listProductsLock {listProductsMutex_};

   // Only list products once per URI
   if (productsReady_)
   {
      return;
   }

   logger_->debug("ListProducts: {}", baseUri_);

   std::deque<std::pair<std::string, cpr::AsyncResponse>> asyncResponses {};
   std::atomic<bool>                                      error {false};

   static constexpr std::size_t kMaxConcurrentRequests = 4u;

   for (const auto& product : kProductDirectoryMap_)
   {
      const std::string productDirectory = product.second;
      const std::string productUrl =
         fmt::format("{0}/{1}", baseUri_, productDirectory);

      // Query to see what radar sites are available for this product
      asyncResponses.emplace_back(
         product.first,
         cpr::GetAsync(cpr::Url {productUrl},
                       network::cpr::GetHeader(),
                       network::cpr::GetDefaultTimeout(),
                       network::cpr::GetDefaultConnectTimeout(),
                       network::cpr::GetDefaultLowSpeed(),
                       network::cpr::GetDefaultProgressCallback(
                          common::ApplicationState::IsRunning())));

      while (asyncResponses.size() >= kMaxConcurrentRequests)
      {
         auto& front = asyncResponses.front();
         ProcessProductDirectory(front.first, front.second, error);
         asyncResponses.pop_front();
      }
   }

   while (!asyncResponses.empty())
   {
      auto& front = asyncResponses.front();
      ProcessProductDirectory(front.first, front.second, error);
      asyncResponses.pop_front();
   }

   if (!error)
   {
      productsReady_ = true;
   }
   else
   {
      availableProducts_.clear();
   }
}

void NwsLevel3SiteData::ProcessProductDirectory(
   const std::string&  product,
   cpr::AsyncResponse& asyncResponse,
   std::atomic<bool>&  error)
{
   const auto response = asyncResponse.get();

   if (response.status_code == cpr::status::HTTP_OK)
   {
      // Parse the response to get the radar sites
      const auto records = network::DirList(response.url.str(), response.text);

      for (const auto& record : records)
      {
         if (record.type_ != std::filesystem::file_type::directory)
         {
            continue;
         }

         // SI.ssss
         static constexpr std::size_t kMinFilenameSize = 7u;

         // Match SI.ssss format
         if (record.filename_.size() >= kMinFilenameSize &&
             record.filename_.starts_with("SI."))
         {
            static constexpr std::size_t kRadarSiteIndex  = 3u;
            static constexpr std::size_t kRadarSiteLength = 4u;

            auto radarSite =
               record.filename_.substr(kRadarSiteIndex, kRadarSiteLength);
            boost::to_upper(radarSite);
            RegisterProduct(product, radarSite);
         }
      }
   }
   else if (response.status_code == 0)
   {
      logger_->warn("Error processing product directory: {} ({})",
                    response.error.message,
                    response.url.str());
      error = true;
   }
}

void NwsLevel3SiteData::RegisterProduct(const std::string& product,
                                        const std::string& radarSite)
{
   const std::unique_lock lock {productsMutex_};
   const auto             it = availableProducts_.find(radarSite);
   if (it == availableProducts_.end())
   {
      // Add new radar site to available products
      availableProducts_.emplace(radarSite, std::vector<std::string> {product});
   }
   else
   {
      // Add product to existing radar site
      it->second.push_back(product);
   }
}

std::vector<std::string> NwsLevel3Behavior::GetAvailableProducts() const
{
   const auto siteData = NwsLevel3SiteData::Instance(p->baseUri_);
   if (siteData)
   {
      return siteData->GetAvailableProducts(p->radarSite_);
   }
   return {};
}

std::vector<std::string>
NwsLevel3SiteData::GetAvailableProducts(const std::string& radarSite)
{
   const std::shared_lock lock {productsMutex_};
   const auto             it = availableProducts_.find(radarSite);
   if (it != availableProducts_.end())
   {
      return it->second;
   }
   return {};
}

bool NwsLevel3Behavior::date_archive_available() const
{
   // Not supported
   return false;
}

std::shared_ptr<NwsLevel3SiteData>
NwsLevel3SiteData::Instance(const std::string& baseUri)
{
   static std::unordered_map<std::string, std::shared_ptr<NwsLevel3SiteData>>
                            instanceMap_;
   static std::shared_mutex instanceMutex_;

   if (baseUri.empty())
   {
      // Return nullptr to indicate that the base URI is empty
      return nullptr;
   }

   std::shared_ptr<NwsLevel3SiteData> instance = nullptr;

   {
      const std::unique_lock lock {instanceMutex_};

      // Look up instance shared pointer
      auto it = instanceMap_.find(baseUri);
      if (it != instanceMap_.end())
      {
         instance = it->second;
      }

      // If no active instance was found, create a new one
      if (instance == nullptr)
      {
         instance = std::make_shared<NwsLevel3SiteData>(baseUri);
         instanceMap_.insert_or_assign(baseUri, instance);
      }
   }

   return instance;
}

} // namespace scwx::provider
