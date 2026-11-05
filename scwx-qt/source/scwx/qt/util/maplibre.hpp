#pragma once

#include <scwx/qt/map/map_context.hpp>

#include <QMapLibre/Map>
#include <QMapLibre/Types>
#include <QPointF>
#include <glm/gtc/type_ptr.hpp>
#include <units/length.h>

#include <memory>

namespace scwx
{
namespace qt
{
namespace util
{
namespace maplibre
{

units::length::meters<double>
          GetMapDistance(const QMapLibre::CustomLayerRenderParameters& params);
glm::mat4 GetMapMatrix(const QMapLibre::CustomLayerRenderParameters& params);
glm::vec2 GetMapScale(const QMapLibre::CustomLayerRenderParameters& params);

/** Overlay uniforms must use the same pixel dimensions as the RHI color
 * texture. */
[[nodiscard]] QMapLibre::CustomLayerRenderParameters
BuildOverlayRenderParameters(const QMapLibre::Map& map,
                             double                renderWidth,
                             double                renderHeight);

/** Ground meters per screen pixel at @p widgetPixel (MapLibre mercator scale).
 */
[[nodiscard]] units::length::meters<double>
MetersPerPixelAt(const std::shared_ptr<QMapLibre::Map>& map,
                 const QPointF&                         widgetPixel);

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

glm::vec2 LatLongToScreenCoordinate(const QMapLibre::Coordinate& coordinate);

void SetMapStyleUrl(const std::shared_ptr<map::MapContext>& mapContext,
                    const std::string&                      url);

/**
 * @brief Find the first layer which should be drawn above the radar products
 *
 * @param [in] styleLayers The layers of the style
 * @param [in] drawBelow A list of RE2 compatible regex's describing the layers
 * to draw below
 *
 * @return The first layer to be drawn above the radar products
 */
std::string FindMapSymbologyLayer(const QStringList&              styleLayers,
                                  const std::vector<std::string>& drawBelow);

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
