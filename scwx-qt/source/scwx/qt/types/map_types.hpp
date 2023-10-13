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
   Information
};

enum class Layer
{
   MapOverlay,
   ColorTable,
   MapSymbology,
   MapUnderlay
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

std::string GetLayerTypeName(LayerType layerType);
std::string GetLayerName(Layer layer);
std::string GetMapTimeName(MapTime mapTime);

} // namespace types
} // namespace qt
} // namespace scwx
