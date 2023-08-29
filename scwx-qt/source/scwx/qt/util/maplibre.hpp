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

/**
 * @brief Determine whether a point lies within a polygon
 *
 * @param [in] vertices Counterclockwise vertices
 * @param [in] point Point to test
 *
 * @return Whether the point lies within the polygon
 */
bool IsPointInPolygon(const std::vector<glm::vec2>& vertices,
                      const glm::vec2&              point);

glm::vec2 LatLongToScreenCoordinate(const QMapLibreGL::Coordinate& coordinate);

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
