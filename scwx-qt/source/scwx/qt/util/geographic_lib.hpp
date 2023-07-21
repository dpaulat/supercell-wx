#pragma once

#include <GeographicLib/Geodesic.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/systems/si/length.hpp>

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
 * @return WGS84 ellipsoid geodesic
 */
const ::GeographicLib::Geodesic& DefaultGeodesic();

/**
 * Get the distance between two points.
 *
 * @param [in] lat1 latitude of point 1 (degrees)
 * @param [in] lon1 longitude of point 1 (degrees)
 * @param [in] lat2 latitude of point 2 (degrees)
 * @param [in] lon2 longitude of point 2 (degrees)
 *
 * @return distance between point 1 and point 2
 */
boost::units::quantity<boost::units::si::length>
GetDistance(double lat1, double lon1, double lat2, double lon2);

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
