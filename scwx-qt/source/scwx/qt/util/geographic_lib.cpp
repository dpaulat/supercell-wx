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

boost::units::quantity<boost::units::si::length>
GetDistance(double lat1, double lon1, double lat2, double lon2)
{
   double distance;
   util::GeographicLib::DefaultGeodesic().Inverse(
      lat1, lon1, lat2, lon2, distance);

   return static_cast<boost::units::quantity<boost::units::si::length>>(
      distance * boost::units::si::meter_base_unit::unit_type());
}

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
