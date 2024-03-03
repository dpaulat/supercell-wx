#include <scwx/qt/util/maplibre.hpp>

#include <QMapLibre/Utils>
#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace maplibre
{

units::length::meters<double>
GetMapDistance(const QMapLibre::CustomLayerRenderParameters& params)
{
   return units::length::meters<double>(
      QMapLibre::metersPerPixelAtLatitude(params.latitude, params.zoom) *
      (params.width + params.height) / 2.0);
}

glm::mat4 GetMapMatrix(const QMapLibre::CustomLayerRenderParameters& params)
{
   glm::vec2 scale = GetMapScale(params);

   glm::mat4 mapMatrix(1.0f);
   mapMatrix = glm::scale(mapMatrix, glm::vec3(scale, 1.0f));
   mapMatrix = glm::rotate(mapMatrix,
                           glm::radians<float>(params.bearing),
                           glm::vec3(0.0f, 0.0f, 1.0f));

   return mapMatrix;
}

glm::vec2 GetMapScale(const QMapLibre::CustomLayerRenderParameters& params)
{
   const float scale = std::pow(2.0, params.zoom) * 2.0f *
                       mbgl::util::tileSize_D / mbgl::util::DEGREES_MAX;
   const float xScale = scale / params.width;
   const float yScale = scale / params.height;

   return glm::vec2 {xScale, yScale};
}

bool IsPointInPolygon(const std::vector<glm::vec2>& vertices,
                      const glm::vec2&              point)
{
   bool inPolygon = true;

   // For each vertex, assume counterclockwise order
   for (std::size_t i = 0; i < vertices.size(); ++i)
   {
      const auto& p1 = vertices[i];
      const auto& p2 =
         (i == vertices.size() - 1) ? vertices[0] : vertices[i + 1];

      // Test which side of edge point lies on
      const float a = -(p2.y - p1.y);
      const float b = p2.x - p1.x;
      const float c = -(a * p1.x + b * p1.y);
      const float d = a * point.x + b * point.y + c;

      // If d < 0, the point is on the right-hand side, and outside of the
      // polygon
      if (d < 0)
      {
         inPolygon = false;
         break;
      }
   }

   return inPolygon;
}

glm::vec2 LatLongToScreenCoordinate(const QMapLibre::Coordinate& coordinate)
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

void SetMapStyleUrl(const std::shared_ptr<map::MapContext>& mapContext,
                    const std::string&                      url)
{
   const auto mapProvider = mapContext->map_provider();

   QString qUrl = QString::fromStdString(url);

   if (mapProvider == map::MapProvider::MapTiler)
   {
      qUrl.append("?key=");
      qUrl.append(map::GetMapProviderApiKey(mapProvider));
   }

   auto map = mapContext->map().lock();
   if (map != nullptr)
   {
      map->setStyleUrl(qUrl);
   }
}

} // namespace maplibre
} // namespace util
} // namespace qt
} // namespace scwx
