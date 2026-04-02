#include <scwx/provider/ondas_level2_data_provider.hpp>
#include <scwx/types/ondas_types.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>
#include <mutex>

#include <re2/re2.h>

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
};

OndasLevel2DataProvider::OndasLevel2DataProvider(const std::string& radarSite,
                                                 const std::string& baseUri) :
    HttpNexradDataProvider(radarSite, baseUri),
    p(std::make_unique<Impl>(this, radarSite, baseUri))
{
}

OndasLevel2DataProvider::~OndasLevel2DataProvider() = default;

OndasLevel2DataProvider::OndasLevel2DataProvider(
   OndasLevel2DataProvider&&) noexcept = default;
OndasLevel2DataProvider& OndasLevel2DataProvider::operator=(
   OndasLevel2DataProvider&&) noexcept = default;

std::chrono::system_clock::time_point
OndasLevel2DataProvider::GetTimePointByKey(const std::string& key) const
{
   return GetTimePointFromKey(key);
}

std::chrono::system_clock::time_point
OndasLevel2DataProvider::GetTimePointFromKey(const std::string& key)
{
   // Filename/Timestamp Format (per ONDAS spec):
   // - Format: YYYYMMDD_HHMM (minimum) or SSSS_YYYYMMDD_HHMM
   // - Example: 20260131_1830 or KILN_20260131_1830
   // - Note: Some servers may include seconds or version suffix

   // The first 8 contiguous digits are used for the data (yyyymmdd), an
   // underscore, the next 4 contiguous digits are the time (hhmm) in 24
   // hour notation. Date and time MUST be in GMT/UTC timezone.

   // June 26, 2005 @ 11:45PM UTC would be 20050626_2145

   static constexpr re2::LazyRE2 re {R"((\d{8}_\d{4}))"};

   std::chrono::system_clock::time_point time {};
   std::string                           dateTimeStr {};

   if (!RE2::PartialMatch(key, *re, &dateTimeStr))
   {
      logger_->warn("Invalid ONDAS timestamp format in key: \"{}\"", key);
      return time;
   }

   // Match now contains the entire "YYYYMMDD_HHMM" substring
   // Parse using std::chrono::parse
   static const std::string timeFormat {"%Y%m%d_%H%M"};
   std::istringstream       ss(dateTimeStr);

   ss >> std::chrono::parse(timeFormat, time);

   if (ss.fail())
   {
      logger_->warn("Failed to parse ONDAS timestamp: \"{}\"", dateTimeStr);
   }

   return time;
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
      const std::unique_lock lock {p->listObjectsMutex_};
      ResetCacheStart();
      ResetCacheFinish();
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
      bool inserted = AddToCache(time, record.filename_, time);
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

   // ONDAS directory listing URL format is:
   // {baseUri}/{radarSite}/dir.list
   return fmt::format("{0}/{1}/dir.list", p->baseUri_, p->radarSite_);
}

std::string OndasLevel2DataProvider::GetFileUrl(const std::string& key)
{
   // ONDAS file URL format is:
   // {baseUri}/{radarSite}/{filename}
   return fmt::format("{0}/{1}/{2}", p->baseUri_, p->radarSite_, key);
}

} // namespace scwx::provider
