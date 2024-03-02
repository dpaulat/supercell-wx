#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/settings/general_settings.hpp>

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
       .providerTemplate_ {
          QMapLibre::Settings::ProviderTemplate::MapboxProvider},
       .mapStyles_ {
          {.name_ {"Streets"},
           .url_ {"mapbox://styles/mapbox/streets-v11"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Basic"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4whef7m000714pc44f3qaxs"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Basic Overcast"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4whev1w002w16s9mgoliotw"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Bubble"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4wxue5j000c14r17uqrjpqb"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Dark"},
           .url_ {"mapbox://styles/mapbox/dark-v10"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Light"},
           .url_ {"mapbox://styles/mapbox/light-v10"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Navigation Guidance Day"},
           .url_ {"mapbox://styles/mapbox/navigation-guidance-day-v4"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Navigation Guidance Night"},
           .url_ {"mapbox://styles/mapbox/navigation-guidance-night-v4"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Outdoors"},
           .url_ {"mapbox://styles/mapbox/outdoors-v11"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Satellite"},
           .url_ {"mapbox://styles/mapbox/satellite-v9"},
           .drawBelow_ {"com.mapbox.annotations.points"}},
          {.name_ {"Satellite Streets"},
           .url_ {"mapbox://styles/mapbox/satellite-streets-v11"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Standard Oil Company"},
           .url_ {
              "mapbox://styles/mapbox-map-design/ckr0svm3922ki18qntevm857n"},
           .drawBelow_ {mapboxDrawBelow_}}}}},
   {MapProvider::MapTiler,
    MapProviderInfo {
       .mapProvider_ {MapProvider::MapTiler},
       .cacheDbName_ {"maptiler-cache.db"},
       .providerTemplate_ {
          QMapLibre::Settings::ProviderTemplate::MapTilerProvider},
       .mapStyles_ {
          {.name_ {"Satellite"},
           .url_ {"https://api.maptiler.com/maps/hybrid/style.json"},
           .drawBelow_ {"tunnel"}},
          {.name_ {"Streets"},
           .url_ {"https://api.maptiler.com/maps/streets-v2/style.json"},
           .drawBelow_ {"aeroway"}},
          {.name_ {"Streets Dark"},
           .url_ {"https://api.maptiler.com/maps/streets-v2-dark/style.json"},
           .drawBelow_ {"aeroway"}},
          {.name_ {"Basic"},
           .url_ {"https://api.maptiler.com/maps/basic-v2/style.json"},
           .drawBelow_ {"railway_transit_tunnel", "Transit tunnel"}},
          {.name_ {"Bright"},
           .url_ {"https://api.maptiler.com/maps/bright-v2/style.json"},
           .drawBelow_ {"ferry"}},
          {.name_ {"Dataviz"},
           .url_ {"https://api.maptiler.com/maps/dataviz/style.json"},
           .drawBelow_ {"aeroway"}},
          {.name_ {"Dataviz Dark"},
           .url_ {"https://api.maptiler.com/maps/dataviz-dark/style.json"},
           .drawBelow_ {"aeroway"}},
          {.name_ {"Outdoor"},
           .url_ {"https://api.maptiler.com/maps/outdoor-v2/style.json"},
           .drawBelow_ {"aeroway_runway", "Aeroway"}},
          {.name_ {"Swisstopo"},
           .url_ {"https://api.maptiler.com/maps/ch-swisstopo-lbm/style.json"},
           .drawBelow_ {"pattern_landcover_vineyard", "Vineyard pattern"}},
          {.name_ {"Swisstopo Dark"},
           .url_ {
              "https://api.maptiler.com/maps/ch-swisstopo-lbm-dark/style.json"},
           .drawBelow_ {"pattern_landcover_vineyard", "Vineyard pattern"}},
          {.name_ {"Swisstopo Grey"},
           .url_ {
              "https://api.maptiler.com/maps/ch-swisstopo-lbm-grey/style.json"},
           .drawBelow_ {"pattern_landcover_vineyard", "Vineyard pattern"}},
          {.name_ {"Swisstopo Vivid"},
           .url_ {"https://api.maptiler.com/maps/ch-swisstopo-lbm-vivid/"
                  "style.json"},
           .drawBelow_ {"pattern_landcover_vineyard", "Vineyard pattern"}},
          {.name_ {"Topo"},
           .url_ {"https://api.maptiler.com/maps/topo-v2/style.json"},
           .drawBelow_ {"aeroway_runway", "Runway"}},
          {.name_ {"Topo Dark"},
           .url_ {"https://api.maptiler.com/maps/topo-v2-dark/style.json"},
           .drawBelow_ {"aeroway_runway", "Runway"}},
          {.name_ {"Winter"},
           .url_ {"https://api.maptiler.com/maps/winter-v2/style.json"},
           .drawBelow_ {"aeroway_runway", "Aeroway"}}}}},
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
      return settings::GeneralSettings::Instance().mapbox_api_key().GetValue();

   case MapProvider::MapTiler:
      return settings::GeneralSettings::Instance()
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
