#include <scwx/qt/types/layer_types.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<LayerType, std::string> layerTypeName_ {
   {LayerType::Map, "Map"},
   {LayerType::Radar, "Radar"},
   {LayerType::Alert, "Alert"},
   {LayerType::Placefile, "Placefile"},
   {LayerType::Information, "Information"},
   {LayerType::Data, "Data"},
   {LayerType::Unknown, "?"}};

static const std::unordered_map<DataLayer, std::string> dataLayerName_ {
   {DataLayer::RadarRange, "Radar Range"}, {DataLayer::Unknown, "?"}};

static const std::unordered_map<InformationLayer, std::string>
   informationLayerName_ {{InformationLayer::MapOverlay, "Map Overlay"},
                          {InformationLayer::ColorTable, "Color Table"},
                          {InformationLayer::Unknown, "?"}};

static const std::unordered_map<MapLayer, std::string> mapLayerName_ {
   {MapLayer::MapSymbology, "Map Symbology"},
   {MapLayer::MapUnderlay, "Map Underlay"},
   {MapLayer::Unknown, "?"}};

LayerType GetLayerType(const std::string& name)
{
   auto result =
      std::find_if(layerTypeName_.cbegin(),
                   layerTypeName_.cend(),
                   [&](const std::pair<LayerType, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != layerTypeName_.cend())
   {
      return result->first;
   }
   else
   {
      return LayerType::Unknown;
   }
}

std::string GetLayerTypeName(LayerType layerType)
{
   return layerTypeName_.at(layerType);
}

DataLayer GetDataLayer(const std::string& name)
{
   auto result =
      std::find_if(dataLayerName_.cbegin(),
                   dataLayerName_.cend(),
                   [&](const std::pair<DataLayer, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != dataLayerName_.cend())
   {
      return result->first;
   }
   else
   {
      return DataLayer::Unknown;
   }
}

std::string GetDataLayerName(DataLayer layer)
{
   return dataLayerName_.at(layer);
}

InformationLayer GetInformationLayer(const std::string& name)
{
   auto result = std::find_if(
      informationLayerName_.cbegin(),
      informationLayerName_.cend(),
      [&](const std::pair<InformationLayer, std::string>& pair) -> bool
      { return boost::iequals(pair.second, name); });

   if (result != informationLayerName_.cend())
   {
      return result->first;
   }
   else
   {
      return InformationLayer::Unknown;
   }
}

std::string GetInformationLayerName(InformationLayer layer)
{
   return informationLayerName_.at(layer);
}

MapLayer GetMapLayer(const std::string& name)
{
   auto result =
      std::find_if(mapLayerName_.cbegin(),
                   mapLayerName_.cend(),
                   [&](const std::pair<MapLayer, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != mapLayerName_.cend())
   {
      return result->first;
   }
   else
   {
      return MapLayer::Unknown;
   }
}

std::string GetMapLayerName(MapLayer layer)
{
   return mapLayerName_.at(layer);
}

} // namespace types
} // namespace qt
} // namespace scwx
