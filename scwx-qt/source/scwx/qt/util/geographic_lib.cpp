#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/util/logger.hpp>

#include <GeographicLib/Gnomonic.hpp>
#include <geos/algorithm/PointLocation.h>
#include <geos/geom/CoordinateSequence.h>

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

units::length::meters<double>
GetDistance(double lat1, double lon1, double lat2, double lon2)
{
   double distance;
   DefaultGeodesic().Inverse(lat1, lon1, lat2, lon2, distance);

   return units::length::meters<double> {distance};
}

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
