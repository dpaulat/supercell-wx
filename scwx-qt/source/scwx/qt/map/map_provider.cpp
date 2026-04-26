#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/settings/general_settings.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx::qt::map
{

static const std::unordered_map<MapProvider, std::string> mapProviderName_ {
   {MapProvider::Mapbox, "Mapbox"},
   {MapProvider::MapTiler, "MapTiler"},
   {MapProvider::OpenFreeMap, "OpenFreeMap"},
   {MapProvider::Unknown, "?"}};

// Draw below tunnels, ferries and roads
static const std::vector<std::string> mapboxDrawBelow_ {
   "tunnel.*", "ferry.*", "road.*"};

static const std::unordered_map<MapProvider, MapProviderInfo> mapProviderInfo_ {
   {MapProvider::Mapbox,
    MapProviderInfo {
       .mapProvider_ = MapProvider::Mapbox,
       .cacheDbName_ {"mbgl-cache.db"},
       .providerTemplate_ =
          QMapLibre::Settings::ProviderTemplate::MapboxProvider,
       .mapStyles_ {
          {.name_ {"Streets"},
           .url_ {"mapbox://styles/mapbox/streets-v11"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"American Memory"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4orrp5e000p14ldwenm7xsf"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Basic"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4whef7m000714pc44f3qaxs"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Basic Overcast"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4whev1w002w16s9mgoliotw"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Blueprint"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cks97e1e37nsd17nzg7p0308g"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Bubble"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4wxue5j000c14r17uqrjpqb"},
           .drawBelow_ {".*\\.annotations\\.points"}},
          {.name_ {"Cali Terrain"},
           .url_ {"mapbox://styles/mapbox/cjerxnqt3cgvp2rmyuxbeqme7"},
           .drawBelow_ {"major roads casing"}},
          {.name_ {"Dark"},
           .url_ {"mapbox://styles/mapbox/dark-v10"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Decimal"},
           .url_ {
              "mapbox://styles/mapbox-map-design/ck4014y110wt61ctt07egsel6"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Frank"},
           .url_ {
              "mapbox://styles/mapbox-map-design/ckshxkppe0gge18nz20i0nrwq"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Ice Cream"},
           .url_ {"mapbox://styles/mapbox/cj7t3i5yj0unt2rmt3y4b5e32"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Le Shine"},
           .url_ {"mapbox://styles/mapbox/cjcunv5ae262f2sm9tfwg8i0w"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Light"},
           .url_ {"mapbox://styles/mapbox/light-v10"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Mineral"},
           .url_ {"mapbox://styles/mapbox/cjtep62gq54l21frr1whf27ak"},
           .drawBelow_ {"tunnel"}},
          {.name_ {"Minimo"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cksjc2nsq1bg117pnekb655h1"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Moonlight"},
           .url_ {"mapbox://styles/mapbox/cj3kbeqzo00022smj7akz3o1e"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Navigation Guidance Day"},
           .url_ {"mapbox://styles/mapbox/navigation-guidance-day-v4"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Navigation Guidance Night"},
           .url_ {"mapbox://styles/mapbox/navigation-guidance-night-v4"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Neon glow"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cl4gxqwi5001415l381n7qwak"},
           .drawBelow_ {"settlement-minor-label"}},
          {.name_ {"North Star"},
           .url_ {"mapbox://styles/mapbox/cj44mfrt20f082snokim4ungi"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Outdoors"},
           .url_ {"mapbox://styles/mapbox/outdoors-v11"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Pencil"},
           .url_ {
              "mapbox://styles/mapbox-map-design/cks9iema71es417mlrft4go2k"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Satellite"},
           .url_ {"mapbox://styles/mapbox/satellite-v9"},
           .drawBelow_ {".*\\.annotations\\.points"}},
          {.name_ {"Satellite Streets"},
           .url_ {"mapbox://styles/mapbox/satellite-streets-v11"},
           .drawBelow_ {mapboxDrawBelow_}},
          {.name_ {"Standard Oil Company"},
           .url_ {
              "mapbox://styles/mapbox-map-design/ckr0svm3922ki18qntevm857n"},
           .drawBelow_ {mapboxDrawBelow_}}}}},
   {MapProvider::MapTiler,
    MapProviderInfo {
       .mapProvider_ = MapProvider::MapTiler,
       .cacheDbName_ {"maptiler-cache.db"},
       .providerTemplate_ =
          QMapLibre::Settings::ProviderTemplate::MapTilerProvider,
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
          {.name_ {"Backdrop"},
           .url_ {"https://api.maptiler.com/maps/backdrop/style.json"},
           .drawBelow_ {"Aeroway"}},
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
          {.name_ {"Landscape"},
           .url_ {"https://api.maptiler.com/maps/landscape/style.json"},
           .drawBelow_ {"Runway"}},
          {.name_ {"Ocean"},
           .url_ {"https://api.maptiler.com/maps/ocean/style.json"},
           .drawBelow_ {"Landform labels"}},
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
          {.name_ {"Toner"},
           .url_ {"https://api.maptiler.com/maps/toner-v2/style.json"},
           .drawBelow_ {"Bridge"}},
          {.name_ {"Topo"},
           .url_ {"https://api.maptiler.com/maps/topo-v2/style.json"},
           .drawBelow_ {"aeroway_runway", "Runway"}},
          {.name_ {"Topo Dark"},
           .url_ {"https://api.maptiler.com/maps/topo-v2-dark/style.json"},
           .drawBelow_ {"aeroway_runway", "Runway"}},
          {.name_ {"Winter"},
           .url_ {"https://api.maptiler.com/maps/winter-v2/style.json"},
           .drawBelow_ {"aeroway_runway", "Aeroway"}}}}},
   {MapProvider::OpenFreeMap,
    MapProviderInfo {
       .mapProvider_ = MapProvider::OpenFreeMap,
       .cacheDbName_ = {"openfreemap-cache.db"},
       .providerTemplate_ =
          QMapLibre::Settings::ProviderTemplate::MapTilerProvider,
       .mapStyles_ {{.name_ {"Liberty"},
                     .url_ {"https://tiles.openfreemap.org/styles/liberty"},
                     .drawBelow_ {"aeroway_runway"}},
                    {.name_ {"Positron"},
                     .url_ {"https://tiles.openfreemap.org/styles/positron"},
                     .drawBelow_ {"building"}},
                    {.name_ {"Bright"},
                     .url_ {"https://tiles.openfreemap.org/styles/bright"},
                     .drawBelow_ {"building"}}}}},
   {MapProvider::Unknown, MapProviderInfo {}}};

bool MapStyle::IsValid() const
{
   return !url_.empty() && !drawBelow_.empty();
}

void ConfigureMapSettings(MapProvider          mapProvider,
                          QMapLibre::Settings& settings)
{
   static constexpr std::size_t kMaxCacheSize =
      20ull * 1024ull * 1024ull; // 20 MB

   const auto& mapProviderInfo = GetMapProviderInfo(mapProvider);

   const std::string localDataPath {
      scwx::qt::main::ApplicationPaths::GetLocation(
         scwx::qt::main::ApplicationPaths::StandardLocation::Local)
         .generic_string()};
   const std::string cacheDbPath {localDataPath + "/" +
                                  mapProviderInfo.cacheDbName_};

   const std::string mapProviderApiKey = map::GetMapProviderApiKey(mapProvider);

   if (mapProvider == map::MapProvider::Mapbox)
   {
      settings.setProviderTemplate(mapProviderInfo.providerTemplate_);
      settings.setApiKey(QString {mapProviderApiKey.c_str()});
   }
   else
   {
      settings.setProviderTemplate(
         QMapLibre::Settings::ProviderTemplate::NoProvider);
      settings.setApiKey({});
   }

   settings.setCacheDatabasePath(QString {cacheDbPath.c_str()});
   settings.setCacheDatabaseMaximumSize(kMaxCacheSize);
}

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
   std::string apiKey {};

   switch (mapProvider)
   {
   case MapProvider::Mapbox:
      apiKey =
         settings::GeneralSettings::Instance().mapbox_api_key().GetValue();
      break;

   case MapProvider::MapTiler:
      apiKey =
         settings::GeneralSettings::Instance().maptiler_api_key().GetValue();
      break;

   case MapProvider::OpenFreeMap:
      apiKey = "";
      break;

   default:
      apiKey = "?";
      break;
   }

   boost::trim(apiKey);

   return apiKey;
}

const MapProviderInfo& GetMapProviderInfo(MapProvider mapProvider)
{
   return mapProviderInfo_.at(mapProvider);
}

} // namespace scwx::qt::map
