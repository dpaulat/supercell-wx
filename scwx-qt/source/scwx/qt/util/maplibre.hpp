#pragma once

#include <QMapLibreGL/types.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <units/length.h>

namespace scwx
{
namespace qt
{
namespace util
{
namespace maplibre
{

units::length::meters<double>
GetMapDistance(const QMapLibreGL::CustomLayerRenderParameters& params);
glm::mat4 GetMapMatrix(const QMapLibreGL::CustomLayerRenderParameters& params);
glm::vec2 GetMapScale(const QMapLibreGL::CustomLayerRenderParameters& params);
glm::vec2 LatLongToScreenCoordinate(const QMapLibreGL::Coordinate& coordinate);

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
