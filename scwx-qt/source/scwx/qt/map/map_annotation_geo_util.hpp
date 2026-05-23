#pragma once

#include <memory>

#include <QPointF>

namespace QMapLibre
{
class Map;
}

namespace scwx::qt::map
{

/** Mean ground meters per screen pixel at @p widgetPixel (1px along X and Y).
 */
[[nodiscard]] double
MetersPerPixelAt(const std::shared_ptr<QMapLibre::Map>& map,
                 const QPointF&                         widgetPixel);

} // namespace scwx::qt::map
