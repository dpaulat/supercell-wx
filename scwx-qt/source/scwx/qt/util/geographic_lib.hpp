#pragma once

#include <GeographicLib/Geodesic.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace GeographicLib
{

/**
 * Get the default geodesic for the WGS84 ellipsoid.
 *
 * return WGS84 ellipsoid geodesic
 */
const ::GeographicLib::Geodesic& DefaultGeodesic();

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
