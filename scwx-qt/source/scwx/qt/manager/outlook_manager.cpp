#include <scwx/qt/manager/outlook_manager.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/status_manager.hpp>
#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/util/network.hpp>
#include <scwx/gr/outlook_placefile.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <QTimer>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::outlook_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Neither SPC's convective/fire outlooks nor WPC's ERO update
// continuously -- each publishes a handful of issuances a day at known
// times -- 20 minutes is a first guess at "frequent enough to notice a
// new issuance reasonably promptly" without hammering either site for a
// file that rarely changes; not tied to either's actual documented
// issuance schedule. One shared interval for every source in Sources()
// below, not tuned per source.
static constexpr int kPollIntervalMs_ = 20 * 60 * 1000;

static const std::string kOutlookCategory_ = "Outlook";

const std::string& OutlookPlacefileCategory()
{ return kOutlookCategory_; }

// Contour rather than fill for every source here, not just WPC's ERO --
// overlapping risk categories are the norm for all of these products (a
// Slight area typically contains a smaller Moderate area, etc.), and
// semi-transparent fills compound into a muddy blend where they overlap;
// outlines read more like how these are conventionally shown. Fixed for
// now, not user-configurable -- a per-outlook persisted toggle (following
// UnitSettings' SettingsCategory pattern) is a real follow-up, not built
// yet.
static constexpr gr::OutlookRenderMode kOutlookRenderMode_ =
   gr::OutlookRenderMode::Contour;
static constexpr unsigned int kContourLineWidth_ = 3;

// WPC's QPF is published only as KML/KMZ, no GeoJSON equivalent
// (confirmed live -- see ConvertOutlookKmzToPlacefile's own doc), unlike
// every SPC source and WPC's own ERO, which are all GeoJSON.
enum class OutlookSourceFormat
{
   GeoJson,
   Kmz
};

// One entry per fetched outlook. SPC's own GeoJSON is self-colored
// (`fill`/`stroke` hex already on each feature -- see
// ConvertOutlookGeoJsonToPlacefile's own doc), so dnColorTable is empty
// for every SPC source; only WPC's ERO needs one; the same generic
// converter handles both cases already, this table just decides which
// gets passed. dnColorTable is unused (and left empty) for Kmz sources --
// KML self-colors via named <Style> elements instead (see
// ConvertOutlookKmzToPlacefile's own doc).
struct OutlookSource
{
   std::string                     url;
   std::string                     title;
   std::string                     cacheFileName;
   std::vector<gr::OutlookDnColor> dnColorTable;
   OutlookSourceFormat             format = OutlookSourceFormat::GeoJson;
};

// Day 1 only for SPC's own outlooks, matching the reference app's own
// single-day picker (see class comment) -- Day 2/3 URL naming is
// confirmed for SPC's categorical outlook only (see
// docs/spc-wpc-outlooks-plan.md's "Not yet started"), not yet verified
// for tornado/wind/hail/fire, so those stay Day 1 here rather than
// guessing at an unverified URL. WPC's own QPF is different: it publishes
// each of Day 1/2/3 individually plus Day 4-5 and Day 6-7 combined (its
// own standard day-by-day breakdown, confirmed live via
// https://www.wpc.ncep.noaa.gov/kml/kmlproducts.php -- WPC has no Day 4/
// Day 5/Day 6/Day 7 *individual* KMZ, only these 5), covering the full
// week the user asked for without guessing at an unpublished breakdown.
const std::vector<OutlookSource>& Sources()
{
   static const std::vector<OutlookSource> sources {
      {"https://www.wpc.ncep.noaa.gov/exper/eromap/geojson/Day1_Latest."
       "geojson",
       "WPC Day 1 Excessive Rainfall Outlook",
       "wpc_ero_day1.txt",
       gr::kWpcEroColorTable},
      {"https://www.spc.noaa.gov/products/outlook/day1otlk_cat.nolyr."
       "geojson",
       "SPC Day 1 Categorical Outlook",
       "spc_day1_categorical.txt",
       {}},
      {"https://www.spc.noaa.gov/products/outlook/day1otlk_torn.nolyr."
       "geojson",
       "SPC Day 1 Tornado Probability",
       "spc_day1_tornado.txt",
       {}},
      {"https://www.spc.noaa.gov/products/outlook/day1otlk_wind.nolyr."
       "geojson",
       "SPC Day 1 Wind Probability",
       "spc_day1_wind.txt",
       {}},
      {"https://www.spc.noaa.gov/products/outlook/day1otlk_hail.nolyr."
       "geojson",
       "SPC Day 1 Hail Probability",
       "spc_day1_hail.txt",
       {}},
      // Two independent sub-hazards, each its own placefile -- matches
      // this manager's "every source independently toggleable" design
      // (see docs/spc-wpc-outlooks-plan.md), not a single combined "Fire"
      // entry.
      {"https://www.spc.noaa.gov/products/fire_wx/day1fw_dryt.nolyr."
       "geojson",
       "SPC Day 1 Fire Weather (Dry Thunderstorm)",
       "spc_day1_fire_dryt.txt",
       {}},
      {"https://www.spc.noaa.gov/products/fire_wx/day1fw_windrh.nolyr."
       "geojson",
       "SPC Day 1 Fire Weather (Wind/RH)",
       "spc_day1_fire_windrh.txt",
       {}},
      {"https://www.wpc.ncep.noaa.gov/kml/qpf/QPF24hr_Day1_latest.kmz",
       "WPC Day 1 QPF",
       "wpc_qpf_day1.txt",
       {},
       OutlookSourceFormat::Kmz},
      {"https://www.wpc.ncep.noaa.gov/kml/qpf/QPF24hr_Day2_latest.kmz",
       "WPC Day 2 QPF",
       "wpc_qpf_day2.txt",
       {},
       OutlookSourceFormat::Kmz},
      {"https://www.wpc.ncep.noaa.gov/kml/qpf/QPF24hr_Day3_latest.kmz",
       "WPC Day 3 QPF",
       "wpc_qpf_day3.txt",
       {},
       OutlookSourceFormat::Kmz},
      {"https://www.wpc.ncep.noaa.gov/kml/qpf/QPF48hr_Day4-5_latest.kmz",
       "WPC Day 4-5 QPF",
       "wpc_qpf_day4_5.txt",
       {},
       OutlookSourceFormat::Kmz},
      {"https://www.wpc.ncep.noaa.gov/kml/qpf/QPF48hr_Day6-7_latest.kmz",
       "WPC Day 6-7 QPF",
       "wpc_qpf_day6_7.txt",
       {},
       OutlookSourceFormat::Kmz},
   };
   return sources;
}

class OutlookManager::Impl
{
public:
   explicit Impl() = default;

   // Same idiom GribManager::Impl/WindBarbManager::Impl use: stop+join in
   // the destructor's *body*, before any implicit member teardown, since
   // fetchPool_'s posted lambda captures `this`.
   ~Impl()
   {
      fetchPool_.stop();
      fetchPool_.join();
   }

   QTimer* timer_ {nullptr};

   boost::asio::thread_pool fetchPool_ {1};
};

OutlookManager::OutlookManager() : p(std::make_unique<Impl>())
{
   p->timer_ = new QTimer(this);
   connect(p->timer_, &QTimer::timeout, this, &OutlookManager::Poll);
   p->timer_->start(kPollIntervalMs_);

   // Deferred for the same reason as GribManager/WindBarbManager's first
   // poll -- Poll() does blocking network I/O, and this manager is
   // constructed during MainWindow startup.
   QTimer::singleShot(0, this, &OutlookManager::Poll);
}

OutlookManager::~OutlookManager() = default;

std::shared_ptr<OutlookManager> OutlookManager::Instance()
{
   static std::weak_ptr<OutlookManager> instanceRef_ {};
   static std::mutex                    instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<OutlookManager> instance = instanceRef_.lock();
   if (instance == nullptr)
   {
      instance     = std::make_shared<OutlookManager>();
      instanceRef_ = instance;
   }

   return instance;
}

void OutlookManager::Poll()
{
   // One post per source rather than one job looping over all of them --
   // fetchPool_ is single-threaded (see Impl), so they still run one at a
   // time, but a slow/hung fetch for one source can't also delay this
   // method's own return, and a status-bar entry per source (see the
   // statusId below) can show independently rather than one combined
   // "Outlooks" entry hiding which specific source is slow.
   for (const OutlookSource& source : Sources())
   {
      boost::asio::post(
         p->fetchPool_,
         [source]()
         {
            auto statusManager         = manager::StatusManager::Instance();
            const std::string statusId = "outlook-" + source.cacheFileName;

            // Binary-safe regardless of format -- cpr::Response::text is
            // just the raw response body, appended byte-for-byte with no
            // text-mode interpretation, so a KMZ (a zip archive) downloads
            // through the exact same call as GeoJSON text.
            auto [downloaded, statusCode] = network::cpr::DownloadToString(
               source.url,
               common::ApplicationState::IsRunning(),
               [&statusManager, &statusId, &source](std::int64_t bytesReceived,
                                                    std::int64_t totalBytes)
               {
                  statusManager->ReportProgress(
                     statusId, source.title, bytesReceived, totalBytes);
               });
            statusManager->ReportComplete(statusId);

            if (statusCode != 200)
            {
               logger_->warn(
                  "Failed to download {} ({})", source.url, statusCode);
               return;
            }

            const std::string placefileText =
               (source.format == OutlookSourceFormat::Kmz) ?
                  gr::ConvertOutlookKmzToPlacefile(downloaded,
                                                   source.title,
                                                   kPollIntervalMs_ / 1000,
                                                   kOutlookRenderMode_,
                                                   kContourLineWidth_) :
                  gr::ConvertOutlookGeoJsonToPlacefile(downloaded,
                                                       source.title,
                                                       kPollIntervalMs_ / 1000,
                                                       source.dnColorTable,
                                                       kOutlookRenderMode_,
                                                       kContourLineWidth_);
            if (placefileText.empty())
            {
               logger_->warn("Could not convert {} to a placefile", source.url);
               return;
            }

            const std::filesystem::path cacheDir =
               main::ApplicationPaths::GetLocation(
                  main::ApplicationPaths::StandardLocation::Cache) /
               "outlooks";
            std::filesystem::create_directories(cacheDir);

            const std::filesystem::path cachePath =
               cacheDir / source.cacheFileName;
            const std::filesystem::path tmpPath = cachePath.string() + ".tmp";

            {
               std::ofstream out {tmpPath, std::ios::binary | std::ios::trunc};
               out << placefileText;
            }

            std::error_code ec;
            std::filesystem::rename(tmpPath, cachePath, ec);
            if (ec)
            {
               logger_->warn(
                  "Could not replace {}: {}", cachePath.string(), ec.message());
               std::filesystem::remove(tmpPath);
               return;
            }

            // AddUrl is a no-op if this path is already registered
            // (built-in and disabled by default -- the user opts in via
            // the same placefile settings UI any other placefile uses);
            // Refresh() forces PlacefileManager to pick up the
            // just-written content rather than waiting on its own
            // refresh timer. Both must use the exact key AddUrl stores
            // under -- PlacefileManager::Impl normalizes internally via
            // util::network::NormalizeUrl and Refresh() looks up by raw
            // string with no normalization of its own, so normalizing
            // here first keeps the two calls consistent regardless of
            // what normalization actually does to a native path
            // (currently a no-op on Linux, but not guaranteed).
            const std::string pathString =
               util::network::NormalizeUrl(cachePath.string());
            auto placefileManager = PlacefileManager::Instance();
            placefileManager->AddUrl(
               pathString, source.title, false, false, kOutlookCategory_);
            // AddUrl no-ops if this path is already registered -- covers
            // a record created before AddUrl even had a category
            // parameter (a real case: this app's own placefiles.json
            // already had the ERO entry from an earlier build,
            // un-tagged).
            placefileManager->set_placefile_category(pathString,
                                                     kOutlookCategory_);
            placefileManager->Refresh(pathString);

            logger_->info("Updated {}", source.title);
         });
   }
}

} // namespace scwx::qt::manager
