#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/manager/settings_manager.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::unordered_map<MapProvider, std::string> mapProviderName_ {
   {MapProvider::Mapbox, "Mapbox"},
   {MapProvider::MapTiler, "MapTiler"},
   {MapProvider::Unknown, "?"}};

// Draw below tunnels, ferries and roads
static const std::vector<std::string> mapboxDrawBelow_ {
   "tunnel.*", "ferry.*", "road.*"};

static const std::unordered_map<MapProvider, MapProviderInfo> mapProviderInfo_ {
   {MapProvider::Mapbox,
    MapProviderInfo {
       .mapProvider_ {MapProvider::Mapbox},
       .cacheDbName_ {"mbgl-cache.db"},
       .settingsTemplate_ {
          QMapLibreGL::Settings::SettingsTemplate::MapboxSettings},
       .mapStyles_ {{.name_ {"Streets"},
                     .url_ {"mapbox://styles/mapbox/streets-v11"},
                     .drawBelow_ {mapboxDrawBelow_}},
                    {.name_ {"Outdoors"},
                     .url_ {"mapbox://styles/mapbox/outdoors-v11"},
                     .drawBelow_ {mapboxDrawBelow_}},
                    {.name_ {"Light"},
                     .url_ {"mapbox://styles/mapbox/light-v10"},
                     .drawBelow_ {mapboxDrawBelow_}},
                    {.name_ {"Dark"},
                     .url_ {"mapbox://styles/mapbox/dark-v10"},
                     .drawBelow_ {mapboxDrawBelow_}},
                    {.name_ {"Satellite"},
                     .url_ {"mapbox://styles/mapbox/satellite-v9"},
                     .drawBelow_ {mapboxDrawBelow_}},
                    {.name_ {"Satellite Streets"},
                     .url_ {"mapbox://styles/mapbox/satellite-streets-v11"},
                     .drawBelow_ {mapboxDrawBelow_}}}}},
   {MapProvider::MapTiler,
    MapProviderInfo {
       .mapProvider_ {MapProvider::MapTiler},
       .cacheDbName_ {"maptiler-cache.db"},
       .settingsTemplate_ {
          QMapLibreGL::Settings::SettingsTemplate::MapTilerSettings},
       .mapStyles_ {{.name_ {"Satellite"},
                     .url_ {"maptiler://maps/hybrid"},
                     .drawBelow_ {"tunnel"}},
                    {.name_ {"Streets"},
                     .url_ {"maptiler://maps/streets-v2"},
                     .drawBelow_ {"aeroway"}},
                    {.name_ {"Basic"},
                     .url_ {"maptiler://maps/basic-v2"},
                     .drawBelow_ {"railway_transit_tunnel"}},
                    {.name_ {"Bright"},
                     .url_ {"maptiler://maps/bright-v2"},
                     .drawBelow_ {"ferry"}},
                    {.name_ {"Outdoor"},
                     .url_ {"maptiler://maps/outdoor-v2"},
                     .drawBelow_ {"aeroway_runway"}},
                    {.name_ {"Topo"},
                     .url_ {"maptiler://maps/topo-v2"},
                     .drawBelow_ {"aeroway_runway"}},
                    {.name_ {"Winter"},
                     .url_ {"maptiler://maps/winter-v2"},
                     .drawBelow_ {"aeroway_runway"}}}}},
   {MapProvider::Unknown, MapProviderInfo {}}};

MapProvider GetMapProvider(const std::string& name)
{
   auto result =
      std::find_if(mapProviderName_.cbegin(),
                   mapProviderName_.cend(),
                   [&](const std::pair<MapProvider, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != mapProviderName_.cend())
   {
      return result->first;
   }
   else
   {
      return MapProvider::Unknown;
   }
}

std::string GetMapProviderName(MapProvider mapProvider)
{
   return mapProviderName_.at(mapProvider);
}

std::string GetMapProviderApiKey(MapProvider mapProvider)
{
   switch (mapProvider)
   {
   case MapProvider::Mapbox:
      return manager::SettingsManager::general_settings()
         .mapbox_api_key()
         .GetValue();

   case MapProvider::MapTiler:
      return manager::SettingsManager::general_settings()
         .maptiler_api_key()
         .GetValue();

   default:
      return "?";
   }
}

const MapProviderInfo& GetMapProviderInfo(MapProvider mapProvider)
{
   return mapProviderInfo_.at(mapProvider);
}

} // namespace map
} // namespace qt
} // namespace scwx
