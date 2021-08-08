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

void RadarRangeLayer::Add(std::shared_ptr<QMapboxGL> map, const QString& before)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Add()";

   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   constexpr float range = 460.0f * 1000.0f;

   constexpr float angleDelta  = 0.5f;
   constexpr float angleDeltaH = angleDelta / 2.0f;

   const QMapbox::Coordinate radar {38.6986, -90.6828};

   float angle = -angleDeltaH;

   QMapbox::Coordinates geometry;

   for (uint16_t azimuth = 0; azimuth <= 720; ++azimuth)
   {
      double latitude;
      double longitude;

      geodesic.Direct(
         radar.first, radar.second, angle, range, latitude, longitude);

      geometry.append({latitude, longitude});

      angle += angleDelta;
   }

   QMapbox::Feature rangeCircle {QMapbox::Feature::LineStringType,
                                 {{geometry}}};

   map->addSource(
      "rangeCircleSource",
      {{"type", "geojson"}, {"data", QVariant::fromValue(rangeCircle)}});
   map->addLayer({{"id", "rangeCircleLayer"},
                  {"type", "line"},
                  {"source", "rangeCircleSource"}},
                 before);
   map->setPaintProperty(
      "rangeCircleLayer", "line-color", "rgba(128, 128, 128, 128)");
}

} // namespace map
} // namespace qt
} // namespace scwx
