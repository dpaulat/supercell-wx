#include <scwx/qt/util/geographic_lib.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace GeographicLib
{

const ::GeographicLib::Geodesic& DefaultGeodesic()
{
   static const ::GeographicLib::Geodesic geodesic_ {
      ::GeographicLib::Constants::WGS84_a(),
      ::GeographicLib::Constants::WGS84_f()};

   return geodesic_;
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
