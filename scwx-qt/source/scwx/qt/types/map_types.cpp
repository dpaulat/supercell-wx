#include <scwx/qt/types/map_types.hpp>

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
   {LayerType::Unknown, "?"}};

static const std::unordered_map<Layer, std::string> layerName_ {
   {Layer::MapOverlay, "Map Overlay"},
   {Layer::ColorTable, "Color Table"},
   {Layer::MapSymbology, "Map Symbology"},
   {Layer::MapUnderlay, "Map Underlay"},
   {Layer::Unknown, "?"}};

static const std::unordered_map<MapTime, std::string> mapTimeName_ {
   {MapTime::Live, "Live"}, {MapTime::Archive, "Archive"}};

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

Layer GetLayer(const std::string& name)
{
   auto result =
      std::find_if(layerName_.cbegin(),
                   layerName_.cend(),
                   [&](const std::pair<Layer, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != layerName_.cend())
   {
      return result->first;
   }
   else
   {
      return Layer::Unknown;
   }
}

std::string GetLayerName(Layer layer)
{
   return layerName_.at(layer);
}

std::string GetMapTimeName(MapTime mapTime)
{
   return mapTimeName_.at(mapTime);
}

} // namespace types
} // namespace qt
} // namespace scwx
