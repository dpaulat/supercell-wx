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

} // namespace GeographicLib
} // namespace util
} // namespace qt
} // namespace scwx
