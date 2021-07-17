#include <scwx/qt/map/radar_range_layer.hpp>

#include <boost/log/trivial.hpp>

#include <glm/glm.hpp>

namespace scwx
{
namespace qt
{

static const std::string logPrefix_ = "[scwx::qt::map::radar_range_layer] ";

void RadarRangeLayer::Add(std::shared_ptr<QMapboxGL> map, const QString& before)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Add()";

   constexpr float range = 460.0f * 1000.0f;

   constexpr float angleDelta  = glm::radians<float>(0.5f);
   constexpr float angleDeltaH = angleDelta / 2.0f;

   const QMapbox::Coordinate      radar {38.6986, -90.6828};
   const QMapbox::ProjectedMeters radarMeters {
      QMapbox::projectedMetersForCoordinate(radar)};

   float angle = -angleDeltaH;

   QMapbox::Coordinates geometry;

   for (uint16_t azimuth = 0; azimuth <= 720; ++azimuth)
   {
      const float sinTheta = std::sinf(angle);
      const float cosTheta = std::cosf(angle);

      const float x = range * sinTheta + radarMeters.second;
      const float y = range * cosTheta + radarMeters.first;

      QMapbox::Coordinate point {QMapbox::coordinateForProjectedMeters({y, x})};

      geometry.append(point);

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

} // namespace qt
} // namespace scwx
