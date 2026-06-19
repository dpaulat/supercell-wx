#include <scwx/qt/types/radar_site_types.hpp>
#include <scwx/util/enum.hpp>

namespace scwx::qt::types
{

static const std::unordered_map<RadarSiteStatus, std::string>
   radarSiteStatusName_ {{RadarSiteStatus::Up, "Up"},
                         {RadarSiteStatus::Warning, "Warning"},
                         {RadarSiteStatus::Down, "Down"},
                         {RadarSiteStatus::HighLatency, "HighLatency"},
                         {RadarSiteStatus::Unknown, "?"}};

static const std::unordered_map<RadarSiteStatus, std::string>
   radarSiteStatusLongName_ {{RadarSiteStatus::Up, "Up"},
                             {RadarSiteStatus::Warning, "Warning"},
                             {RadarSiteStatus::Down, "Down"},
                             {RadarSiteStatus::HighLatency, "High Latency"},
                             {RadarSiteStatus::Unknown, "Unknown"}};

static const std::unordered_map<RadarSiteStatus, std::string>
   radarSiteStatusDescription_ {
      {RadarSiteStatus::Up, "Data received within last 5 minutes"},
      {RadarSiteStatus::Warning, "Data received more than 5 minutes ago"},
      {RadarSiteStatus::Down, "Data received more than 30 minutes ago"},
      {RadarSiteStatus::HighLatency,
       "Data took more than 60 seconds to arrive"},
      {RadarSiteStatus::Unknown, "No radar site status available"}};

SCWX_GET_ENUM(RadarSiteStatus, GetRadarSiteStatus, radarSiteStatusName_)

const std::string& GetRadarSiteStatusName(RadarSiteStatus status)
{
   return radarSiteStatusName_.at(status);
}

const std::string& GetRadarSiteStatusLongName(RadarSiteStatus status)
{
   return radarSiteStatusLongName_.at(status);
}

const std::string& GetRadarSiteStatusDescription(RadarSiteStatus status)
{
   return radarSiteStatusDescription_.at(status);
}

} // namespace scwx::qt::types
