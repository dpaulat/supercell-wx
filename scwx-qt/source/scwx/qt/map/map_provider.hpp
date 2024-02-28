#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

#include <QMapLibre/Settings>

namespace scwx
{
namespace qt
{
namespace map
{

enum class MapProvider
{
   Mapbox,
   MapTiler,
   Unknown
};
typedef scwx::util::
   Iterator<MapProvider, MapProvider::Mapbox, MapProvider::MapTiler>
      MapProviderIterator;

struct MapStyle
{
   std::string              name_;
   std::string              url_;
   std::vector<std::string> drawBelow_;
};

struct MapProviderInfo
{
   MapProvider                           mapProvider_ {MapProvider::Unknown};
   std::string                           cacheDbName_ {};
   QMapLibre::Settings::SettingsTemplate settingsTemplate_ {};
   std::vector<MapStyle>                 mapStyles_ {};
};

MapProvider            GetMapProvider(const std::string& name);
std::string            GetMapProviderName(MapProvider mapProvider);
std::string            GetMapProviderApiKey(MapProvider mapProvider);
const MapProviderInfo& GetMapProviderInfo(MapProvider mapProvider);

} // namespace map
} // namespace qt
} // namespace scwx
