#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/common/sites.hpp>

#include <unordered_map>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace config
{

static const std::string logPrefix_ = "[scwx::qt::settings::radar_site] ";

static const std::string defaultRadarSiteFile_ =
   ":/res/config/radar_sites.json";

static std::unordered_map<std::string, std::shared_ptr<RadarSite>>
                                                    radarSiteMap_;
static std::unordered_map<std::string, std::string> siteIdMap_;

static bool ValidateJsonEntry(const boost::json::object& o);

class RadarSiteImpl
{
public:
   explicit RadarSiteImpl() :
       type_ {},
       id_ {},
       latitude_ {0.0},
       longitude_ {0.0},
       country_ {},
       state_ {},
       place_ {}
   {
   }

   ~RadarSiteImpl() {}

   std::string type_;
   std::string id_;
   double      latitude_;
   double      longitude_;
   std::string country_;
   std::string state_;
   std::string place_;
};

RadarSite::RadarSite() : p(std::make_unique<RadarSiteImpl>()) {}
RadarSite::~RadarSite() = default;

RadarSite::RadarSite(RadarSite&&) noexcept = default;
RadarSite& RadarSite::operator=(RadarSite&&) noexcept = default;

std::string RadarSite::type() const
{
   return p->type_;
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

std::shared_ptr<RadarSite> RadarSite::Get(const std::string& id)
{
   std::shared_ptr<RadarSite> radarSite = nullptr;

   if (radarSiteMap_.contains(id))
   {
      radarSite = radarSiteMap_.at(id);
   }

   return radarSite;
}

std::string GetRadarIdFromSiteId(const std::string& siteId)
{
   std::string id = "???";

   if (siteIdMap_.contains(siteId))
   {
      id = siteIdMap_.at(siteId);
   }

   return id;
}

void RadarSite::Initialize()
{
   ReadConfig(defaultRadarSiteFile_);
}

size_t RadarSite::ReadConfig(const std::string& path)
{
   BOOST_LOG_TRIVIAL(info) << logPrefix_ << "Loading radar sites from \""
                           << path << "\"...";

   bool   dataValid  = true;
   size_t sitesAdded = 0;

   boost::json::value j = util::json::ReadJsonFile(path);

   dataValid = j.is_array();

   if (dataValid)
   {
      for (auto& v : j.as_array())
      {
         auto& o = v.as_object();

         if (!ValidateJsonEntry(o))
         {
            BOOST_LOG_TRIVIAL(info) << logPrefix_ << "Incorrect format: " << v;
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
               BOOST_LOG_TRIVIAL(warning)
                  << logPrefix_ << "Site ID conflict: " << siteIdMap_.at(siteId)
                  << " and " << site->p->id_;
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
