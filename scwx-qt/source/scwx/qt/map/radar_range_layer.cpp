#include <scwx/qt/map/radar_range_layer.hpp>

#include <boost/log/trivial.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <glm/glm.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "[scwx::qt::map::radar_range_layer] ";

static std::shared_ptr<QMapbox::Feature>
GetRangeCircle(float range, QMapbox::Coordinate center);

void RadarRangeLayer::Add(std::shared_ptr<QMapboxGL> map,
                          float                      range,
                          QMapbox::Coordinate        center,
                          const QString&             before)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Add()";

   if (map->layerExists("rangeCircleLayer"))
   {
      map->removeLayer("rangeCircleLayer");
   }
   if (map->sourceExists("rangeCircleSource"))
   {
      map->removeSource("rangeCircleSource");
   }

   std::shared_ptr<QMapbox::Feature> rangeCircle =
      GetRangeCircle(range, center);

   map->addSource(
      "rangeCircleSource",
      {{"type", "geojson"}, {"data", QVariant::fromValue(*rangeCircle)}});
   map->addLayer({{"id", "rangeCircleLayer"},
                  {"type", "line"},
                  {"source", "rangeCircleSource"}},
                 before);
   map->setPaintProperty(
      "rangeCircleLayer", "line-color", "rgba(128, 128, 128, 128)");
}

void RadarRangeLayer::Update(std::shared_ptr<QMapboxGL> map,
                             float                      range,
                             QMapbox::Coordinate        center)
{
   std::shared_ptr<QMapbox::Feature> rangeCircle =
      GetRangeCircle(range, center);

   map->updateSource("rangeCircleSource",
                     {{"data", QVariant::fromValue(*rangeCircle)}});
}

static std::shared_ptr<QMapbox::Feature>
GetRangeCircle(float range, QMapbox::Coordinate center)
{
   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   constexpr float angleDelta  = 0.5f;
   constexpr float angleDeltaH = angleDelta / 2.0f;

   float angle = -angleDeltaH;

   QMapbox::Coordinates geometry;

   for (uint16_t azimuth = 0; azimuth <= 720; ++azimuth)
   {
      double latitude;
      double longitude;

      geodesic.Direct(center.first,
                      center.second,
                      angle,
                      range * 1000.0f,
                      latitude,
                      longitude);

      geometry.append({latitude, longitude});

      angle += angleDelta;
   }

   std::shared_ptr<QMapbox::Feature> rangeCircle =
      std::make_shared<QMapbox::Feature>(
         QMapbox::Feature::LineStringType,
         std::initializer_list<QMapbox::CoordinatesCollection> {
            std::initializer_list<QMapbox::Coordinates> {geometry}});

   return rangeCircle;
}

} // namespace map
} // namespace qt
} // namespace scwx
