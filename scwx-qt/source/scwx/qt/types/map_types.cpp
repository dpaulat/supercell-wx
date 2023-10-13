#include <scwx/qt/types/map_types.hpp>

#include <unordered_map>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<types::LayerType, std::string> layerTypeName_ {
   {types::LayerType::Map, "Map"},
   {types::LayerType::Radar, "Radar"},
   {types::LayerType::Alert, "Alert"},
   {types::LayerType::Placefile, "Placefile"},
   {types::LayerType::Information, "Information"}};

static const std::unordered_map<types::Layer, std::string> layerName_ {
   {types::Layer::MapOverlay, "Map Overlay"},
   {types::Layer::ColorTable, "Color Table"},
   {types::Layer::MapSymbology, "Map Symbology"},
   {types::Layer::MapUnderlay, "Map Underlay"}};

static const std::unordered_map<MapTime, std::string> mapTimeName_ {
   {MapTime::Live, "Live"}, {MapTime::Archive, "Archive"}};

std::string GetLayerTypeName(LayerType layerType)
{
   return layerTypeName_.at(layerType);
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
