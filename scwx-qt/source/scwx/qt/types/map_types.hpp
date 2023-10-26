#pragma once

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
   Unknown
};

enum class Layer
{
   MapOverlay,
   ColorTable,
   MapSymbology,
   MapUnderlay,
   Unknown
};

enum class AnimationState
{
   Play,
   Pause
};

enum class MapTime
{
   Live,
   Archive
};

enum class NoUpdateReason
{
   NoChange,
   NotLoaded,
   InvalidProduct,
   InvalidData
};

LayerType   GetLayerType(const std::string& name);
std::string GetLayerTypeName(LayerType layerType);

Layer       GetLayer(const std::string& name);
std::string GetLayerName(Layer layer);

std::string GetMapTimeName(MapTime mapTime);

} // namespace types
} // namespace qt
} // namespace scwx
