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

RadarSiteStatus    GetRadarSiteStatus(const std::string& name);
const std::string& GetRadarSiteStatusName(RadarSiteStatus status);
const std::string& GetRadarSiteStatusLongName(RadarSiteStatus status);
const std::string& GetRadarSiteStatusDescription(RadarSiteStatus status);

} // namespace scwx::qt::types
