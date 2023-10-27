#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class LayerType
{
   Map,
   Radar,
   Alert,
   Placefile,
   Information,
   Data,
   Unknown
};

enum class DataLayer
{
   RadarRange,
   Unknown
};
typedef scwx::util::
   Iterator<DataLayer, DataLayer::RadarRange, DataLayer::RadarRange>
      DataLayerIterator;

enum class InformationLayer
{
   MapOverlay,
   ColorTable,
   Unknown
};

enum class MapLayer
{
   MapSymbology,
   MapUnderlay,
   Unknown
};

LayerType   GetLayerType(const std::string& name);
std::string GetLayerTypeName(LayerType layerType);

DataLayer   GetDataLayer(const std::string& name);
std::string GetDataLayerName(DataLayer layer);

InformationLayer GetInformationLayer(const std::string& name);
std::string      GetInformationLayerName(InformationLayer layer);

MapLayer    GetMapLayer(const std::string& name);
std::string GetMapLayerName(MapLayer layer);

} // namespace types
} // namespace qt
} // namespace scwx
