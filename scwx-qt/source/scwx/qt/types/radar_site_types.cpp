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

static const std::unordered_map<RadarType, std::string> radarTypeName_ {
   {RadarType::Research, "research"},
   {RadarType::FAA, "faa"},
   {RadarType::WSR88D, "wsr88d"},
   {RadarType::TDWR, "tdwr"},
   {RadarType::Unknown, "?"}};

static const std::unordered_map<RadarType, std::string> radarTypeLongName_ {
   {RadarType::Research, "Research"},
   {RadarType::FAA, "FAA"},
   {RadarType::WSR88D, "WSR-88D"},
   {RadarType::TDWR, "TDWR"},
   {RadarType::Unknown, "?"}};

static const std::unordered_map<std::uint32_t, RadarType> radarTypeCodeMap_ {
   {1, RadarType::Research},
   {13, RadarType::FAA},
   {29, RadarType::WSR88D},
   {3, RadarType::TDWR},
   {35, RadarType::TDWR}};

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

SCWX_GET_ENUM(RadarType, GetRadarType, radarTypeName_)

RadarType GetRadarType(std::uint32_t code)
{
   RadarType type = RadarType::Unknown;

   const auto it = radarTypeCodeMap_.find(code);
   if (it != radarTypeCodeMap_.end())
   {
      type = it->second;
   }

   return type;
}

const std::string& GetRadarTypeName(RadarType type)
{
   return radarTypeName_.at(type);
}

const std::string& GetRadarTypeLongName(RadarType type)
{
   return radarTypeLongName_.at(type);
}

} // namespace scwx::qt::types
