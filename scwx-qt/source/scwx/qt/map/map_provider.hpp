#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

#include <QMapLibre/Settings>

namespace scwx::qt::map
{

enum class MapProvider
{
   Mapbox,
   MapTiler,
   Unknown
};
using MapProviderIterator = scwx::util::
   Iterator<MapProvider, MapProvider::Mapbox, MapProvider::MapTiler>;

struct MapStyle
{
   std::string              name_;
   std::string              url_;
   std::vector<std::string> drawBelow_;

   [[nodiscard]] bool IsValid() const;
};

struct MapProviderInfo
{
   MapProvider                           mapProvider_ {MapProvider::Unknown};
   std::string                           cacheDbName_ {};
   QMapLibre::Settings::ProviderTemplate providerTemplate_ {};
   std::vector<MapStyle>                 mapStyles_ {};
};

void                   ConfigureMapSettings(MapProvider          mapProvider,
                                            QMapLibre::Settings& settings);
MapProvider            GetMapProvider(const std::string& name);
std::string            GetMapProviderName(MapProvider mapProvider);
std::string            GetMapProviderApiKey(MapProvider mapProvider);
const MapProviderInfo& GetMapProviderInfo(MapProvider mapProvider);

} // namespace scwx::qt::map
