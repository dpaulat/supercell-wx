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

bool AreaInRangeOfPoint(const std::vector<common::Coordinate>& area,
                        const common::Coordinate&              point,
                        const units::length::meters<double>    distance)
{
   /*
   Uses the gnomonic projection to determine if the area is in the radius.

   The first property needed to make this work is that great circles become
   lines in the projection.
   The other key property needed to make this work is described bellow
      R1 and R2 are the distances from the center point to two points
      on the (non-flat) Earth.
      R1' and R2' are the distances from the center point to the same
      two points in the gnomonic projection.
      if R1 > R2 then
         R1' > R2'
      else if R1 < R2 then
         R1' < R2'
      else if R1 == R2 then
         R1' == R2'

      This can also be written as:
      r(d) is a function that takes the distance on Earth and converts it to a
      distance on the projection.
      R1' = r(R1), R2' = r(R2)
      r(d) is increasing

   In this case, R1 is a point the radius away from the center, and R2 is a
   (all of the) point(s) on the edge of the area. This means that if the edge
   is in the radius R1' on the projection, it is in the radius R1 on the Earth.

   On a spherical geodesic this works fine. R is the radius of Earth. We are
   also only concerned with points less than a hemisphere away, therefore
   0 < R1,R2 < pi/2 * R (quarter of circumference because the point is in the
   center of the hemisphere)
      r(d) = R * tan(d / R) {0 < d < pi/2 * R}
      tan(d / R) is increasing for {0 < d < pi/2 * R}

   On non spherical geodesics, this may not work perfectly, but should be a
   close approximation.
   */
   // Cannot have an area with just two points
   if (area.size() <= 2 || (area.size() == 3 && area.front() == area.back()))
   {
      return false;
   }

   // Ensure that the same geodesic is used here as is for the radius
   // calculation
   ::GeographicLib::Gnomonic gnomonic =
      ::GeographicLib::Gnomonic(DefaultGeodesic());
   geos::geom::CoordinateSequence sequence {};
   double                         x;
   double                         y;

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
      // Check if the current point is the hemisphere centered on the point
      if (std::isnan(x) || std::isnan(y))
      {
         return false;
      }
      sequence.add(x, y);
   }

   // get a point on the circle with the radius of the range in lat lon.
   // Has the point be in the general direction of the area, which may help with
   // non spherical geodesics
   units::angle::degrees<double> angle = GetAngle(
      point.latitude_, point.longitude_, area[0].latitude_, area[0].longitude_);
   common::Coordinate radiusPoint = GetCoordinate(point, angle, distance);
   // get the radius in gnomonic projection
   gnomonic.Forward(point.latitude_,
                    point.longitude_,
                    radiusPoint.latitude_,
                    radiusPoint.longitude_,
                    x,
                    y);
   // radius is greater than quarter circumference of the Earth, but the area
   // is closer, so it is in range.
   if (std::isnan(x) || std::isnan(y))
   {
      return true;
   }
   double gnomonicRadius = std::sqrt(x * x + y * y);

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
         if (geos::algorithm::PointLocation::isInRing(zero, &sequence))
         {
            return true;
         }
         else if (distance > units::length::meters<double>(0))
         {
            // Calculate the distance the area is from the point via conversion
            // to a polygon.
            auto geometryFactory =
               geos::geom::GeometryFactory::getDefaultInstance();
            auto linearRing = geometryFactory->createLinearRing(sequence);
            auto polygon =
               geometryFactory->createPolygon(std::move(linearRing));

            geos::algorithm::distance::PointPairDistance distancePair;
            geos::algorithm::distance::DistanceToPoint::computeDistance(
               *polygon, zero, distancePair);
            return gnomonicRadius >= distancePair.getDistance();
         }
      }
      catch (const std::exception&)
      {
         logger_->trace("Invalid area sequence");
      }
   }

   return false;
}

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
