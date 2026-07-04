#pragma once

#include <scwx/util/iterator.hpp>

#include <cstdint>
#include <string>

namespace scwx::qt::types
{

enum class RadarSiteStatus : std::uint8_t
{
   Up,          // < 5 minutes
   Warning,     // < 30 minutes
   Down,        // >= 30 minutes
   HighLatency, // > 60 seconds
   Unknown
};
using RadarSiteStatusIterator = scwx::util::
   Iterator<RadarSiteStatus, RadarSiteStatus::Up, RadarSiteStatus::Unknown>;

enum class RadarType : std::uint8_t
{
   Research,
   FAA,
   WSR88D,
   TDWR,
   Unknown
};
using RadarTypeIterator =
   scwx::util::Iterator<RadarType, RadarType::Research, RadarType::Unknown>;

RadarSiteStatus    GetRadarSiteStatus(const std::string& name);
const std::string& GetRadarSiteStatusName(RadarSiteStatus status);
const std::string& GetRadarSiteStatusLongName(RadarSiteStatus status);
const std::string& GetRadarSiteStatusDescription(RadarSiteStatus status);

RadarType          GetRadarType(const std::string& name);
RadarType          GetRadarType(std::uint32_t code);
const std::string& GetRadarTypeName(RadarType type);
const std::string& GetRadarTypeLongName(RadarType type);

} // namespace scwx::qt::types
