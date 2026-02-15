#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iterator>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/json.hpp>
#include <boost/tokenizer.hpp>
#include <units/length.h>
#include <QFile>
#include <QTextStream>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

namespace scwx::qt::config
{

static const std::string logPrefix_ = "scwx::qt::config::radar_site";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string defaultRadarSiteFile_ =
   ":/res/config/radar_sites.json";

static std::unordered_map<std::string, std::shared_ptr<RadarSite>>
                                                    radarSiteMap_;
static std::vector<std::shared_ptr<RadarSite>>      radarSiteList_;
static std::unordered_map<std::string, std::string> siteIdMap_;
static std::shared_mutex                            siteMutex_;

// Time zones for known custom radar sites
static const std::unordered_map<std::string, std::string> timeZoneMap_ {
   {"DAN1", "America/Chicago"},
   {"DOP1", "America/Chicago"},
   {"FOP1", "America/Chicago"},
   {"FUSA", "America/New_York"},
   {"FWLX", "America/Chicago"},
   {"GAWX", "America/New_York"},
   {"K08D", "America/Chicago"},
   {"KOUN", "America/Chicago"},
   {"KULM", "America/Chicago"},
   {"KXWA", "America/Chicago"},
   {"MZZU", "America/Chicago"},
   {"NOP3", "America/Chicago"},
   {"NOP4", "America/Chicago"},
   {"OP5R", "America/Anchorage"},
   {"ROP3", "America/Chicago"},
   {"ROP4", "America/Chicago"},
   {"WILU", "America/Chicago"}};

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

   static std::size_t ReadGisConfig(const std::string& filePath);
   static std::size_t ReadJsonConfig(const std::string& filePath);

   types::RadarType            type_ {types::RadarType::Unknown};
   std::string                 id_ {};
   double                      latitude_ {0.0};
   double                      longitude_ {0.0};
   std::string                 country_ {};
   std::string                 state_ {};
   std::string                 place_ {};
   std::string                 tzName_ {};
   units::length::feet<double> altitude_ {0.0};

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

types::RadarType RadarSite::type() const
{
   return p->type_;
}

std::string RadarSite::type_name() const
{
   return types::GetRadarTypeLongName(p->type_);
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

   if (p->country_ == "USA" || p->country_.empty())
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
   std::shared_lock lock(siteMutex_);
   return radarSiteList_;
}

std::shared_ptr<RadarSite>
RadarSite::FindNearest(double                                latitude,
                       double                                longitude,
                       const std::optional<types::RadarType> type,
                       bool                                  includeDown,
                       bool includeDecommissioned)
{
   std::shared_lock lock(siteMutex_);

   double distanceInMeters = 0.0;

   std::shared_ptr<RadarSite> nearestRadarSite = nullptr;
   double                     nearestDistance  = 0.0;

   for (const auto& radarSite : radarSiteList_)
   {
      // If the type filter doesn't match, skip
      if (type.has_value() && radarSite->type() != *type)
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

std::size_t RadarSite::ReadConfig(const std::string& path)
{
   logger_->info("Loading radar sites from \"{}\"...", path);

   std::size_t sitesAdded = 0;

   if (boost::iends_with(path, ".json"))
   {
      sitesAdded = Impl::ReadJsonConfig(path);
   }
   else if (boost::iends_with(path, ".gis"))
   {
      sitesAdded = Impl::ReadGisConfig(path);
   }
   else
   {
      logger_->warn("Unsupported radar site config format: \"{}\"", path);
   }

   return sitesAdded;
}

std::size_t RadarSite::Impl::ReadGisConfig(const std::string& filePath)
{
   // CSV fields:
   // 0: ID (e.g. "MZZU")
   // 1: Parent ID (e.g. "MZZU")
   // 2: Latitude (e.g. "34.161")
   // 3: Longitude (e.g. "-84.143")
   // 4: Elevation in meters (e.g. "305.0")
   // 5: Type ID (e.g. "1" for Research, "29" for WSR-88D, etc.)
   // 6: State (e.g. "GA")
   // 7: Description (e.g. "Some description")
   enum class Field : std::uint8_t
   {
      Id = 0,
      ParentId,
      Latitude,
      Longitude,
      Elevation,
      TypeId,
      State,
      Description
   };

   static constexpr std::size_t kNumFields = 8;
   std::size_t                  sitesAdded = 0;

   QFile file {QString::fromStdString(filePath)};
   if (!file.open(QIODevice::ReadOnly))
   {
      logger_->error("Failed to open radar site config file: \"{}\"", filePath);
      return sitesAdded;
   }

   std::string                   line;
   std::string                   id;
   std::string                   parentId;
   double                        latitude {};
   double                        longitude {};
   units::length::meters<double> elevation;
   std::uint32_t                 typeId {};
   std::string                   state;
   std::string                   description;

   using tokenizer = boost::tokenizer<boost::escaped_list_separator<char>>;
   static const boost::escaped_list_separator<char> separator {
      '\\',  // Escape character
      ',',   // Separator character
      '\"'}; // Quote character

   QTextStream fileStream(&file);
   fileStream.setEncoding(QStringConverter::Utf8);

   std::string        fileSource = fileStream.readAll().toStdString();
   std::istringstream is {fileSource};

   while (scwx::util::getline(is, line))
   {
      boost::trim(line);
      if (line.empty())
      {
         continue; // Skip empty lines
      }

      const tokenizer   tokens {line, separator};
      const std::size_t tokenCount =
         std::distance(tokens.begin(), tokens.end());

      if (tokenCount < kNumFields)
      {
         logger_->warn(
            "Invalid line in GIS radar site config (expected at least {} "
            "fields): \"{}\"",
            kNumFields,
            line);
         continue;
      }

      std::vector<std::string> fields {};
      fields.reserve(std::distance(tokens.begin(), tokens.end()));
      std::ranges::transform(tokens,
                             std::back_inserter(fields),
                             [](const std::string& s)
                             { return boost::trim_copy(s); });

      try
      {
         id       = fields[static_cast<std::size_t>(Field::Id)];
         parentId = fields[static_cast<std::size_t>(Field::ParentId)];
         latitude =
            std::stod(fields[static_cast<std::size_t>(Field::Latitude)]);
         longitude =
            std::stod(fields[static_cast<std::size_t>(Field::Longitude)]);
         elevation = units::length::meters<double> {
            std::stod(fields[static_cast<std::size_t>(Field::Elevation)])};
         typeId = static_cast<std::uint32_t>(
            std::stoul(fields[static_cast<std::size_t>(Field::TypeId)]));
         state       = fields[static_cast<std::size_t>(Field::State)];
         description = fields[static_cast<std::size_t>(Field::Description)];
      }
      catch (const std::exception& ex)
      {
         logger_->warn(
            "Error parsing line in GIS radar site config: \"{}\". Error: {}",
            line,
            ex.what());
         continue;
      }

      logger_->trace(
         "Parsed GIS radar site: id={}, parentId={}, lat={}, lon={}, elev={}, "
         "typeId={}, state={}, description={}",
         id,
         parentId,
         latitude,
         longitude,
         elevation.value(),
         typeId,
         state,
         description);

      boost::to_upper(id);

      // Create RadarSite object and add to map
      auto radarSite           = std::make_shared<RadarSite>();
      radarSite->p->id_        = id;
      radarSite->p->type_      = types::GetRadarType(typeId);
      radarSite->p->latitude_  = latitude;
      radarSite->p->longitude_ = longitude;
      radarSite->p->altitude_  = elevation;
      radarSite->p->state_     = state;
      radarSite->p->place_     = description; // Using description as place

      const auto timeZoneIt = timeZoneMap_.find(id);
      if (timeZoneIt != timeZoneMap_.end())
      {
         radarSite->p->tzName_ = timeZoneIt->second;
      }
      else
      {
         // GIS config doesn't include time zone info
         radarSite->p->tzName_ = "UTC";
      }

      try
      {
#if (__cpp_lib_chrono >= 201907L)
         using namespace std::chrono;
#else
         using namespace date;
#endif

         radarSite->p->timeZone_ =
            get_tzdb().locate_zone(radarSite->p->tzName_);
      }
      catch (const std::runtime_error&)
      {
         logger_->warn("{} unknown time zone: {}",
                       radarSite->p->id_,
                       radarSite->p->tzName_);
      }

      const std::unique_lock lock(siteMutex_);

      if (!radarSiteMap_.contains(id))
      {
         radarSiteMap_[id] = radarSite;
         radarSiteList_.push_back(radarSite);
         ++sitesAdded;
      }
      else
      {
         logger_->debug("Duplicate radar site ID found in GIS config: \"{}\"",
                        id);
      }

      const std::string siteId = common::GetSiteId(id);

      if (!siteIdMap_.contains(siteId))
      {
         siteIdMap_[siteId] = id;
      }
      else if (siteIdMap_.at(siteId) != id)
      {
         logger_->warn(
            "Site ID conflict: {} and {}", siteIdMap_.at(siteId), id);
      }
   }

   return sitesAdded;
}

std::size_t RadarSite::Impl::ReadJsonConfig(const std::string& filePath)
{
   bool        dataValid  = true;
   std::size_t sitesAdded = 0;

   const boost::json::value j = util::json::ReadJsonQFile(filePath);

   dataValid = j.is_array();

   if (dataValid)
   {
      const std::unique_lock lock(siteMutex_);

      for (const auto& v : j.as_array())
      {
         const auto& o = v.as_object();

         if (!ValidateJsonEntry(o))
         {
            logger_->info("Incorrect format: {}", boost::json::serialize(v));
         }
         else
         {
            std::shared_ptr<RadarSite> site = std::make_shared<RadarSite>();

            site->p->type_ = types::GetRadarType(
               boost::json::value_to<std::string>(o.at("type")));
            site->p->id_       = boost::json::value_to<std::string>(o.at("id"));
            site->p->latitude_ = boost::json::value_to<double>(o.at("lat"));
            site->p->longitude_ = boost::json::value_to<double>(o.at("lon"));
            site->p->country_ =
               boost::json::value_to<std::string>(o.at("country"));
            site->p->state_ = boost::json::value_to<std::string>(o.at("state"));
            site->p->place_ = boost::json::value_to<std::string>(o.at("place"));
            site->p->tzName_   = boost::json::value_to<std::string>(o.at("tz"));
            site->p->altitude_ = units::length::feet<double> {
               boost::json::value_to<double>(o.at("elevation"))};

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
               radarSiteList_.push_back(site);
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

} // namespace scwx::qt::config
