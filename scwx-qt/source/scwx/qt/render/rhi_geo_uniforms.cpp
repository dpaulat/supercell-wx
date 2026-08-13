#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/projection.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/time.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace scwx::qt::render
{

GeoUniforms
BuildGeoUniforms(const QMapLibre::CustomLayerRenderParameters& params,
                 const bool                                    thresholded,
                 const std::chrono::system_clock::time_point   selectedTime)
{
   GeoUniforms uniforms {};

   uniforms.uMVPMatrix = OrthoMapProjection(params);
   uniforms.uMVPMatrix =
      glm::rotate(uniforms.uMVPMatrix,
                  glm::radians<float>(static_cast<float>(params.bearing)),
                  glm::vec3(0.0f, 0.0f, 1.0f));

   const glm::vec2 mapScale = util::maplibre::GetMapScale(params);
   uniforms.uMapMatrix =
      glm::scale(glm::mat4 {1.0f}, glm::vec3(mapScale.x, -mapScale.y, 1.0f));
   uniforms.uMapMatrix =
      glm::rotate(uniforms.uMapMatrix,
                  glm::radians<float>(static_cast<float>(params.bearing)),
                  glm::vec3(0.0f, 0.0f, 1.0f));
   uniforms.uOriginLatLong = glm::vec2 {static_cast<float>(params.latitude),
                                        static_cast<float>(params.longitude)};

   if (thresholded)
   {
      uniforms.uMapDistance =
         static_cast<float>(util::maplibre::GetMapDistance(params).value());
   }

   const std::chrono::system_clock::time_point effectiveTime =
      (selectedTime == std::chrono::system_clock::time_point {}) ?
         scwx::util::time::now() :
         selectedTime;

   uniforms.uSelectedTime = static_cast<std::int32_t>(
      std::chrono::duration_cast<std::chrono::minutes>(
         effectiveTime.time_since_epoch())
         .count());

   return uniforms;
}

} // namespace scwx::qt::render
