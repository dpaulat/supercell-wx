#include <scwx/qt/util/maplibre.hpp>

#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace maplibre
{

glm::vec2 LatLongToScreenCoordinate(const QMapLibreGL::Coordinate& coordinate)
{
   static constexpr double RAD2DEG_D = 180.0 / M_PI;

   double latitude = std::clamp(
      coordinate.first, -mbgl::util::LATITUDE_MAX, mbgl::util::LATITUDE_MAX);
   glm::vec2 screen {
      mbgl::util::LONGITUDE_MAX + coordinate.second,
      -(mbgl::util::LONGITUDE_MAX -
        RAD2DEG_D *
           std::log(std::tan(M_PI / 4.0 +
                             latitude * M_PI / mbgl::util::DEGREES_MAX)))};
   return screen;
}

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
