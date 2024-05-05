#include <scwx/qt/types/location_types.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<LocationMethod, std::string>
   locationMethodName_ {{LocationMethod::Fixed, "Fixed"},
                        {LocationMethod::Track, "Track"},
                        {LocationMethod::County, "County"},
                        {LocationMethod::All, "All"},
                        {LocationMethod::Unknown, "?"}};

static const std::unordered_map<PositioningPlugin, std::string>
   positioningPluginName_ {{PositioningPlugin::Default, "Default"},
                           {PositioningPlugin::Nmea, "NMEA"},
                           {PositioningPlugin::Unknown, "?"}};

SCWX_GET_ENUM(LocationMethod, GetLocationMethod, locationMethodName_)
SCWX_GET_ENUM(PositioningPlugin, GetPositioningPlugin, positioningPluginName_)

const std::string& GetLocationMethodName(LocationMethod locationMethod)
{
   return locationMethodName_.at(locationMethod);
}

const std::string& GetPositioningPluginName(PositioningPlugin positioningPlugin)
{
   return positioningPluginName_.at(positioningPlugin);
}

} // namespace types
} // namespace qt
} // namespace scwx
