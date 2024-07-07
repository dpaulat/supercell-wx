#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/util/logger.hpp>

#include <numbers>

#include <GeographicLib/Gnomonic.hpp>
#include <geos/algorithm/PointLocation.h>
#include <geos/algorithm/distance/PointPairDistance.h>
#include <geos/algorithm/distance/DistanceToPoint.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/GeometryFactory.h>

namespace scwx
{
namespace qt
{
namespace util
{
namespace GeographicLib
{

static const std::string logPrefix_ = "scwx::qt::util::geographic_lib";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

const ::GeographicLib::Geodesic& DefaultGeodesic()
{
   static const ::GeographicLib::Geodesic geodesic_ {
      ::GeographicLib::Constants::WGS84_a(),
      ::GeographicLib::Constants::WGS84_f()};

   return geodesic_;
}

bool GnomonicAreaContainsCenter(geos::geom::CoordinateSequence sequence)
{

   // Cannot have an area with just two points
   if (sequence.size() <= 2 ||
       (sequence.size() == 3 && sequence.front() == sequence.back()))
   {
      return false;
   }
   bool areaContainsPoint = false;
   geos::geom::CoordinateXY zero {};
   // If the sequence is not a ring, add the first point again for closure
   if (!sequence.isRing())
   {
      sequence.add(sequence.front(), false);
   }

   // The sequence should be a ring at this point, but make sure
   if (sequence.isRing())
   {
      try
      {
         areaContainsPoint =
            geos::algorithm::PointLocation::isInRing(zero, &sequence);
      }
      catch (const std::exception&)
      {
         logger_->trace("Invalid area sequence");
      }
   }

   return areaContainsPoint;
}

bool AreaContainsPoint(const std::vector<common::Coordinate>& area,
                       const common::Coordinate&              point)
{
   // Cannot have an area with just two points
   if (area.size() <= 2 || (area.size() == 3 && area.front() == area.back()))
   {
      return false;
   }

   ::GeographicLib::Gnomonic      gnomonic {};
   geos::geom::CoordinateSequence sequence {};
   double                         x;
   double                         y;
   bool                           areaContainsPoint = false;

   // Using a gnomonic projection with the test point as the center
   // latitude/longitude, the projected test point will be at (0, 0)
   geos::geom::CoordinateXY zero {};

   // Create the area coordinate sequence using a gnomonic projection
   for (auto& areaCoordinate : area)
   {
      gnomonic.Forward(point.latitude_,
                       point.longitude_,
                       areaCoordinate.latitude_,
                       areaCoordinate.longitude_,
                       x,
                       y);
      sequence.add(x, y);
   }

   // If the sequence is not a ring, add the first point again for closure
   if (!sequence.isRing())
   {
      sequence.add(sequence.front(), false);
   }

   // The sequence should be a ring at this point, but make sure
   if (sequence.isRing())
   {
      try
      {
         areaContainsPoint =
            geos::algorithm::PointLocation::isInRing(zero, &sequence);
      }
      catch (const std::exception&)
      {
         logger_->trace("Invalid area sequence");
      }
   }

   return areaContainsPoint;
}







units::angle::degrees<double>
GetAngle(double lat1, double lon1, double lat2, double lon2)
{
   double azi1;
   double azi2;
   DefaultGeodesic().Inverse(lat1, lon1, lat2, lon2, azi1, azi2);

   return units::angle::degrees<double> {azi1};
}

common::Coordinate GetCoordinate(const common::Coordinate&     center,
                                 units::angle::degrees<double> angle,
                                 units::length::meters<double> distance)
{
   double latitude;
   double longitude;

   DefaultGeodesic().Direct(center.latitude_,
                            center.longitude_,
                            angle.value(),
                            distance.value(),
                            latitude,
                            longitude);

   return {latitude, longitude};
}

common::Coordinate GetCoordinate(const common::Coordinate& center,
                                 units::meters<double>     i,
                                 units::meters<double>     j)
{
   // Calculate polar coordinates based on i and j
   const double angle =
      std::atan2(i.value(), j.value()) * 180.0 / std::numbers::pi;
   const double range =
      std::sqrt(i.value() * i.value() + j.value() * j.value());

   double latitude;
   double longitude;

   DefaultGeodesic().Direct(
      center.latitude_, center.longitude_, angle, range, latitude, longitude);

   return {latitude, longitude};
}

units::length::meters<double>
GetDistance(double lat1, double lon1, double lat2, double lon2)
{
   double distance;
   DefaultGeodesic().Inverse(lat1, lon1, lat2, lon2, distance);

   return units::length::meters<double> {distance};
}

/*
 * Uses the gnomonic projection to determine if the area is in the radius.
 *
 * The basic algorithm is as follows:
 *    - Get a gnomonic projection centered on the point of the area
 *    - Find the point on the area which is closest to the center
 *    - Convert the closest point back to latitude and longitude.
 *    - Find the distance form the closest point to the point.
 *
 * The first property needed to make this work is that great circles become
 * lines in the projection, which allows the area to be converted to strait
 * lines. This is generally true for gnomic projections.
 *
 * The second property needed to make this work is that a point further away
 * on the geodesic must be further away on the projection. This means that
 * the closes point on the projection is also the closest point on the geodesic.
 * This holds for spherical geodesics and is an approximation non spherical
 * geodesics.
 *
 * This algorithm only works if the area is fully on the hemisphere centered
 * on the point. Otherwise, this falls back to centroid based distances.
 *
 * If the point is inside the area, 0 is always returned.
 */
units::length::meters<double>
GetDistanceAreaPoint(const std::vector<common::Coordinate>& area,
                     const common::Coordinate&              point)
{
   // Ensure that the same geodesic is used here as is for the distance
   // calculation
   ::GeographicLib::Gnomonic gnomonic =
      ::GeographicLib::Gnomonic(DefaultGeodesic());
   geos::geom::CoordinateSequence sequence {};
   double                         x;
   double                         y;
   bool                           useCentroid = false;

   // Using a gnomonic projection with the test point as the center
   // latitude/longitude, the projected test point will be at (0, 0)
   geos::geom::CoordinateXY zero {};

   // Create the area coordinate sequence using a gnomonic projection
   for (auto& areaCoordinate : area)
   {
      gnomonic.Forward(point.latitude_,
                       point.longitude_,
                       areaCoordinate.latitude_,
                       areaCoordinate.longitude_,
                       x,
                       y);
      // Check if the current point is in the hemisphere centered on the point
      // if not, fall back to using centroid.
      if (std::isnan(x) || std::isnan(y))
      {
         useCentroid = true;
      }
      sequence.add(x, y);
   }

   units::length::meters<double> distance;

   if (useCentroid)
   {
      common::Coordinate centroid = common::GetCentroid(area);
      distance = GetDistance(point.latitude_,
                             point.longitude_,
                             centroid.latitude_,
                             centroid.longitude_);
   }
   else if (GnomonicAreaContainsCenter(sequence))
   {
      distance = units::length::meters<double>(0);
   }
   else
   {
      // Get the closes point on the geometry
      auto geometryFactory = geos::geom::GeometryFactory::getDefaultInstance();
      auto lineString      = geometryFactory->createLineString(sequence);

      geos::algorithm::distance::PointPairDistance distancePair;
      geos::algorithm::distance::DistanceToPoint::computeDistance(
         *lineString, zero, distancePair);

      geos::geom::CoordinateXY closestPoint = distancePair.getCoordinate(0);

      double closestLat;
      double closestLon;

      gnomonic.Reverse(point.latitude_,
                       point.longitude_,
                       closestPoint.x,
                       closestPoint.y,
                       closestLat,
                       closestLon);

      distance = GetDistance(point.latitude_,
                             point.longitude_,
                             closestLat,
                             closestLon);
   }
   return distance;
}

bool AreaInRangeOfPoint(const std::vector<common::Coordinate>& area,
                        const common::Coordinate&              point,
                        const units::length::meters<double>    distance)
{
    return GetDistanceAreaPoint(area, point) <= distance;
}

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
