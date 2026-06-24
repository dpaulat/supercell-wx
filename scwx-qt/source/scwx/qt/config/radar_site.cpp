#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <unordered_map>

#include <boost/json.hpp>

#if (__cpp_lib_chrono < 201907L)
#include <date/date.h>
#endif

namespace scwx
{
namespace qt
{
namespace config
{

static const std::string logPrefix_ = "scwx::qt::config::radar_site";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string defaultRadarSiteFile_ =
   ":/res/config/radar_sites.json";

static const std::unordered_map<std::string, std::string> typeNameMap_ {
   {"wsr88d", "WSR-88D"}, {"tdwr", "TDWR"}, {"?", "?"}};

static std::unordered_map<std::string, std::shared_ptr<RadarSite>>
                                                    radarSiteMap_;
static std::unordered_map<std::string, std::string> siteIdMap_;
static std::shared_mutex                            siteMutex_;

static bool ValidateJsonEntry(const boost::json::object& o);

class RadarSite::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::string type_ {};
   std::string id_ {};
   double      latitude_ {0.0};
   double      longitude_ {0.0};
   std::string country_ {};
   std::string state_ {};
   std::string place_ {};
   std::string tzName_ {};
   double      altitude_ {0.0};

   std::atomic<std::chrono::system_clock::time_point> lastReceived_ {};
   std::atomic<std::chrono::milliseconds>             latency_ {};
   std::atomic<types::RadarSiteStatus>                status_ {
      types::RadarSiteStatus::Unknown};

   const scwx::util::time_zone* timeZone_ {nullptr};
};

RadarSite::RadarSite() : p(std::make_unique<Impl>()) {}
RadarSite::~RadarSite() = default;

RadarSite::RadarSite(RadarSite&&) noexcept            = default;
RadarSite& RadarSite::operator=(RadarSite&&) noexcept = default;

std::string RadarSite::type() const
{
   return p->type_;
}

std::string RadarSite::type_name() const
{
   auto it = typeNameMap_.find(p->type_);
   if (it != typeNameMap_.cend())
   {
      return it->second;
   }
   else
   {
      return typeNameMap_.at("?");
   }
}

std::string RadarSite::id() const
{
   return p->id_;
}

double RadarSite::latitude() const
{
   return p->latitude_;
}

double RadarSite::longitude() const
{
   return p->longitude_;
}

std::string RadarSite::country() const
{
   return p->country_;
}

std::string RadarSite::state() const
{
   return p->state_;
}

std::string RadarSite::place() const
{
   return p->place_;
}

std::string RadarSite::location_name() const
{
   std::string locationName;

   if (p->country_ == "USA")
   {
      locationName = fmt::format("{}, {}", p->place_, p->state_);
   }
   else if (std::all_of(p->state_.cbegin(),
                        p->state_.cend(),
                        [](char c) { return std::isdigit(c); }))
   {
      locationName = fmt::format("{}, {}", p->place_, p->country_);
   }
   else
   {
      locationName =
         fmt::format("{}, {}, {}", p->place_, p->state_, p->country_);
   }

   return locationName;
}

std::string RadarSite::tz_name() const
{
   return p->tzName_;
}

const scwx::util::time_zone* RadarSite::time_zone() const
{
   return p->timeZone_;
}

units::length::feet<double> RadarSite::altitude() const
{
   return units::length::feet<double>(p->altitude_);
}

std::chrono::system_clock::time_point RadarSite::last_received() const
{
   return p->lastReceived_;
}

std::chrono::milliseconds RadarSite::latency() const
{
   return p->latency_;
}

scwx::qt::types::RadarSiteStatus RadarSite::status() const
{
   return p->status_;
}

void RadarSite::set_last_received(
   std::chrono::system_clock::time_point lastReceived)
{
   p->lastReceived_ = lastReceived;
}

void RadarSite::set_latency(std::chrono::milliseconds latency)
{
   p->latency_ = latency;
}

void RadarSite::set_status(types::RadarSiteStatus status)
{
   p->status_ = status;
}

std::shared_ptr<RadarSite> RadarSite::Get(const std::string& id)
{
   std::shared_lock           lock(siteMutex_);
   std::shared_ptr<RadarSite> radarSite = nullptr;

   if (radarSiteMap_.contains(id))
   {
      radarSite = radarSiteMap_.at(id);
   }

   return radarSite;
}

std::vector<std::shared_ptr<RadarSite>> RadarSite::GetAll()
{
   std::shared_lock                        lock(siteMutex_);
   std::vector<std::shared_ptr<RadarSite>> radarSites;

   radarSites.reserve(radarSiteMap_.size());

   for (const auto& site : radarSiteMap_)
   {
      radarSites.push_back(site.second);
   }

   return radarSites;
}

std::shared_ptr<RadarSite>
RadarSite::FindNearest(double                            latitude,
                       double                            longitude,
                       const std::optional<std::string>& type,
                       bool                              includeDown,
                       bool                              includeDecommissioned)
{
   std::shared_lock lock(siteMutex_);

   double distanceInMeters = 0.0;

   std::shared_ptr<RadarSite> nearestRadarSite = nullptr;
   double                     nearestDistance  = 0.0;

   for (const auto& site : radarSiteMap_)
   {
      auto& radarSite = site.second;

      // If the type filter doesn't match, skip
      if (type.has_value() && radarSite->type() != type)
      {
         continue;
      }

      // Optionally exclude radars which are currently down.
      if (!includeDown && radarSite->status() == types::RadarSiteStatus::Down)
      {
         continue;
      }

      // Optionally exclude KLIX from automatic selection.
      if (!includeDecommissioned && radarSite->id() == "KLIX")
      {
         continue;
      }

      // Calculate distance to radar site
      util::GeographicLib::DefaultGeodesic().Inverse(latitude,
                                                     longitude,
                                                     radarSite->latitude(),
                                                     radarSite->longitude(),
                                                     distanceInMeters);

      // If the radar site is the closer, record it as the closest
      if (nearestRadarSite == nullptr || distanceInMeters < nearestDistance)
      {
         nearestRadarSite = radarSite;
         nearestDistance  = distanceInMeters;
      }
   }

   return nearestRadarSite;
}

std::string GetRadarIdFromSiteId(const std::string& siteId)
{
   std::shared_lock lock(siteMutex_);
   std::string      id = "???";

   if (siteIdMap_.contains(siteId))
   {
      id = siteIdMap_.at(siteId);
   }

   return id;
}

void RadarSite::Initialize()
{
   static bool initialized_ = false;

   if (!initialized_)
   {
      ReadConfig(defaultRadarSiteFile_);
      initialized_ = true;
   }
}

size_t RadarSite::ReadConfig(const std::string& path)
{
   logger_->info("Loading radar sites from \"{}\"...", path);

   bool   dataValid  = true;
   size_t sitesAdded = 0;

   boost::json::value j = util::json::ReadJsonQFile(path);

   dataValid = j.is_array();

   if (dataValid)
   {
      std::unique_lock lock(siteMutex_);

      for (auto& v : j.as_array())
      {
         auto& o = v.as_object();

         if (!ValidateJsonEntry(o))
         {
            logger_->info("Incorrect format: {}", boost::json::serialize(v));
         }
         else
         {
            std::shared_ptr<RadarSite> site = std::make_shared<RadarSite>();

            site->p->type_ = boost::json::value_to<std::string>(o.at("type"));
            site->p->id_   = boost::json::value_to<std::string>(o.at("id"));
            site->p->latitude_  = boost::json::value_to<double>(o.at("lat"));
            site->p->longitude_ = boost::json::value_to<double>(o.at("lon"));
            site->p->country_ =
               boost::json::value_to<std::string>(o.at("country"));
            site->p->state_ = boost::json::value_to<std::string>(o.at("state"));
            site->p->place_ = boost::json::value_to<std::string>(o.at("place"));
            site->p->tzName_ = boost::json::value_to<std::string>(o.at("tz"));
            site->p->altitude_ =
               boost::json::value_to<double>(o.at("elevation"));

            try
            {
#if (__cpp_lib_chrono >= 201907L)
               using namespace std::chrono;
#else
               using namespace date;
#endif

               site->p->timeZone_ = get_tzdb().locate_zone(site->p->tzName_);
            }
            catch (const std::runtime_error&)
            {
               logger_->warn(
                  "{} unknown time zone: {}", site->p->id_, site->p->tzName_);
            }

            if (!radarSiteMap_.contains(site->p->id_))
            {
               radarSiteMap_[site->p->id_] = site;
               ++sitesAdded;
            }

            std::string siteId = common::GetSiteId(site->p->id_);

            if (!siteIdMap_.contains(siteId))
            {
               siteIdMap_[siteId] = site->p->id_;
            }
            else
            {
               logger_->warn("Site ID conflict: {} and {}",
                             siteIdMap_.at(siteId),
                             site->p->id_);
            }
         }
      }
   }

   return sitesAdded;
}

static bool ValidateJsonEntry(const boost::json::object& o)
{
   return (o.contains("type") && o.at("type").is_string() &&       //
           o.contains("id") && o.at("id").is_string() &&           //
           o.contains("lat") && o.at("lat").is_double() &&         //
           o.contains("lon") && o.at("lon").is_double() &&         //
           o.contains("country") && o.at("country").is_string() && //
           o.contains("state") && o.at("state").is_string() &&     //
           o.contains("place") && o.at("place").is_string());
}

} // namespace config
} // namespace qt
} // namespace scwx
