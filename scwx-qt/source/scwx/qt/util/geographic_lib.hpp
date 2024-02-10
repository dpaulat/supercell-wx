#pragma once

#include <scwx/common/geographic.hpp>

#include <vector>

#include <GeographicLib/Geodesic.hpp>
#include <units/angle.h>
#include <units/length.h>

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
 * Determine if an area/ring, oriented in either direction, contains a point. A
 * point lying on the area boundary is considered to be inside the area.
 *
 * @param [in] area A vector of Coordinates representing the area
 * @param [in] point The point to check against the area
 *
 * @return true if point is inside the area
 */
bool AreaContainsPoint(const std::vector<common::Coordinate>& area,
                       const common::Coordinate&              point);

/**
 * Get the angle between two points.
 *
 * @param [in] lat1 latitude of point 1 (degrees)
 * @param [in] lon1 longitude of point 1 (degrees)
 * @param [in] lat2 latitude of point 2 (degrees)
 * @param [in] lon2 longitude of point 2 (degrees)
 *
 * @return angle between point 1 and point 2
 */
units::angle::degrees<double>
GetAngle(double lat1, double lon1, double lat2, double lon2);

/**
 * Get a coordinate from an (i, j) offset.
 *
 * @param [in] center The center coordinate from which i and j are offset
 * @param [in] i The easting offset in meters
 * @param [in] j The northing offset in meters
 * 
 * @return offset coordinate
 */
common::Coordinate GetCoordinate(const common::Coordinate& center,
                                 units::meters<double>     i,
                                 units::meters<double>     j);

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
units::length::meters<double>
GetDistance(double lat1, double lon1, double lat2, double lon2);

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
