#pragma once

#include <QMapLibreGL/types.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace maplibre
{

glm::vec2 LatLongToScreenCoordinate(const QMapLibreGL::Coordinate& coordinate);

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
