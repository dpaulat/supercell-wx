#include <scwx/qt/map/map_annotation_geo_util.hpp>
#include <scwx/qt/util/geographic_lib.hpp>

#include <QMapLibre/Map>

namespace scwx::qt::map
{

double MetersPerPixelAt(const std::shared_ptr<QMapLibre::Map>& map,
                        const QPointF&                         widgetPixel)
{
   if (map == nullptr)
   {
      return 0.0;
   }

   const auto metersAlong = [&](double dx, double dy) -> double
   {
      const auto c0 = map->coordinateForPixel(widgetPixel);
      const auto c1 = map->coordinateForPixel(
         QPointF {widgetPixel.x() + dx, widgetPixel.y() + dy});
      return util::GeographicLib::GetDistance(
                c0.first, c0.second, c1.first, c1.second)
         .value();
   };

   const double mppX = metersAlong(1.0, 0.0);
   const double mppY = metersAlong(0.0, 1.0);
   return 0.5 * (mppX + mppY);
}

} // namespace scwx::qt::map
