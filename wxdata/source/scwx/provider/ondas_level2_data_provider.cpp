#include <scwx/provider/ondas_level2_data_provider.hpp>
#include <scwx/config/ondas_config_loader.hpp>
#include <scwx/types/ondas_types.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>
#include <mutex>

namespace scwx::provider
{

static const std::string logPrefix_ =
   "scwx::provider::ondas_level2_data_provider";
static const auto logger_ = util::Logger::Create(logPrefix_);

class OndasLevel2DataProvider::Impl
{
public:
   explicit Impl(OndasLevel2DataProvider* self,
                 std::string              radarSite,
                 std::string              baseUri) :
       self_(self),
       radarSite_(std::move(radarSite)),
       baseUri_(std::move(baseUri))
   {
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   OndasLevel2DataProvider* self_;

   std::string radarSite_;
   std::string baseUri_;

   std::mutex listObjectsMutex_ {};

   std::mutex                                 ondasConfigMutex_ {};
   std::shared_ptr<const config::OndasConfig> ondasConfig_ {};
};

OndasLevel2DataProvider::OndasLevel2DataProvider(const std::string& radarSite,
                                                 const std::string& baseUri) :
    HttpNexradDataProvider(radarSite, baseUri),
    p(std::make_unique<Impl>(this, radarSite, baseUri))
{
}

OndasLevel2DataProvider::~OndasLevel2DataProvider() = default;

std::chrono::system_clock::time_point
OndasLevel2DataProvider::GetTimePointByKey(const std::string& key) const
{
   return GetTimePointFromKey(key);
}

std::chrono::system_clock::time_point
OndasLevel2DataProvider::GetTimePointFromKey(const std::string& key)
{
   return config::OndasConfig::GetTimePointFromFilename(key);
}

std::tuple<bool, size_t, size_t>
OndasLevel2DataProvider::ListObjects(std::chrono::system_clock::time_point date)
{
   const std::string listingUrl = GetListingUrl(date);
   logger_->debug("ListObjects: {}", listingUrl);

   // Download dir.list
   const std::string content = DownloadToString(listingUrl);
   if (content.empty())
   {
      return {false, 0, 0};
   }

   // Parse ONDAS format
   const auto records = types::ondas::ParseOndasDirList(content);

   std::size_t newObjects   = 0;
   std::size_t totalObjects = 0;

   const std::unique_lock lock {p->listObjectsMutex_};

   ResetCacheStart();

   for (const auto& record : records)
   {
      const auto time = GetTimePointFromKey(record.filename_);
      if (time == std::chrono::system_clock::time_point {})
      {
         continue; // Invalid timestamp
      }

      // Add to cache (key is the filename)
      // Note: ONDAS doesn't provide lastModified, use file time as
      // approximation
      const bool inserted = AddToCache(time, record.filename_, time);
      if (inserted)
      {
         newObjects++;
      }
      totalObjects++;
   }

   ResetCacheFinish();

   return {true, newObjects, totalObjects};
}

std::string OndasLevel2DataProvider::GetListingUrl(
   std::chrono::system_clock::time_point date)
{
   (void) date; // Not needed since ONDAS dir.list contains all dates

   // Default list file
   std::string listFile = "dir.list";

   // Get ONDAS config if not already loaded
   {
      const std::unique_lock lock {p->ondasConfigMutex_};
      if (!p->ondasConfig_)
      {
         const auto result = config::OndasConfigLoader::Get(p->baseUri_);
         p->ondasConfig_   = result.config;
      }
   }

   // Use ONDAS config if available
   if (p->ondasConfig_)
   {
      listFile = p->ondasConfig_->list_file();
   }
   else
   {
      logger_->debug("No ONDAS config for {}", p->baseUri_);
   }

   // ONDAS directory listing URL format is:
   // {baseUri}/{radarSite}/dir.list
   return fmt::format("{0}/{1}/{2}", p->baseUri_, p->radarSite_, listFile);
}

std::string OndasLevel2DataProvider::GetFileUrl(const std::string& key)
{
   // ONDAS file URL format is:
   // {baseUri}/{radarSite}/{filename}
   return fmt::format("{0}/{1}/{2}", p->baseUri_, p->radarSite_, key);
}

} // namespace scwx::provider
