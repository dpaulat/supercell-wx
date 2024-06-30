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
namespace util {
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
   // Cannot have an area with just two points
   if (area.size() <= 2 || (area.size() == 3 && area.front() == area.back()))
   {
      return false;
   }



   ::GeographicLib::Gnomonic      gnomonic =
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
      sequence.add(x, y);
   }

   // get a point on the circle with the radius of the range in lat lon.
   units::angle::degrees<double> angle = units::angle::degrees<double>(0);
   common::Coordinate radiusPoint = GetCoordinate(point, angle, distance);
   // get the radius in gnomonic projection
   gnomonic.Forward(point.latitude_,
                    point.longitude_,
                    radiusPoint.latitude_,
                    radiusPoint.longitude_,
                    x,
                    y);
   double gnomonicRadius = sqrt(x * x + y * y);

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

         // Calculate the distance the point is from the output
         geos::algorithm::distance::PointPairDistance distancePair;
         auto geometryFactory =
            geos::geom::GeometryFactory::getDefaultInstance();
         auto linearRing = geometryFactory->createLinearRing(sequence);
         auto polygon    =
            geometryFactory->createPolygon(std::move(linearRing));
         geos::algorithm::distance::DistanceToPoint::computeDistance(*polygon,
                                                                     zero,
                                                                     distancePair);
         if (gnomonicRadius > distancePair.getDistance())
         {
            return true;
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
