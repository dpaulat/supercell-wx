#pragma once

#include <QMapLibreGL/types.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/systems/si/length.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace maplibre
{

boost::units::quantity<boost::units::si::length>
GetMapDistance(const QMapLibreGL::CustomLayerRenderParameters& params);
glm::vec2 LatLongToScreenCoordinate(const QMapLibreGL::Coordinate& coordinate);

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
