#include <scwx/qt/render/projection.hpp>
#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/util/maplibre.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace scwx::qt::render
{

static constexpr std::size_t kMapColorFloatsPerVertex_   = 8;
static constexpr std::size_t kMapColorIntegersPerVertex_ = 3;
static constexpr std::size_t kColoredFloatsPerVertex_    = 7;
static constexpr std::size_t kVerticesPerTriangle_       = 3;

static bool IsMapColorTriangleVisible(std::int32_t threshold,
                                      std::int32_t startTime,
                                      std::int32_t endTime,
                                      float        mapDistance,
                                      std::int32_t selectedTime)
{
   if (!(threshold == 0 || mapDistance == 0.0f ||
         (threshold < 0 && static_cast<float>(-threshold) <= mapDistance) ||
         static_cast<float>(threshold) >= mapDistance || threshold >= 999))
   {
      return false;
   }

   if (startTime != 0 && (startTime > selectedTime || selectedTime >= endTime))
   {
      return false;
   }

   return true;
}

glm::mat4
OrthoMapProjection(const QMapLibre::CustomLayerRenderParameters& params)
{
   return glm::ortho(0.0f,
                     static_cast<float>(params.width),
                     0.0f,
                     static_cast<float>(params.height));
}

std::vector<float> TransformMapColorVertices(
   const std::vector<float>&                     floatVertices,
   const std::vector<std::int32_t>&              integerVertices,
   const QMapLibre::CustomLayerRenderParameters& params,
   const bool                                    thresholded,
   const std::chrono::system_clock::time_point   selectedTime)
{
   if (floatVertices.empty())
   {
      return {};
   }

   const GeoUniforms uniforms =
      BuildGeoUniforms(params, thresholded, selectedTime);
   const glm::vec2 mapScreenCoord = util::maplibre::LatLongToScreenCoordinate(
      {params.latitude, params.longitude});

   const std::size_t vertexCount =
      floatVertices.size() / kMapColorFloatsPerVertex_;
   std::vector<float> transformed;
   transformed.reserve(vertexCount * kColoredFloatsPerVertex_);

   for (std::size_t tri = 0; tri < vertexCount / kVerticesPerTriangle_; ++tri)
   {
      const std::size_t baseVertex = tri * kVerticesPerTriangle_;
      const std::size_t intOffset  = baseVertex * kMapColorIntegersPerVertex_;

      if (!IsMapColorTriangleVisible(integerVertices[intOffset],
                                     integerVertices[intOffset + 1],
                                     integerVertices[intOffset + 2],
                                     uniforms.uMapDistance,
                                     uniforms.uSelectedTime))
      {
         continue;
      }

      for (std::size_t v = 0; v < kVerticesPerTriangle_; ++v)
      {
         const std::size_t floatOffset =
            (baseVertex + v) * kMapColorFloatsPerVertex_;

         const glm::vec2 p {floatVertices[floatOffset + 0],
                            floatVertices[floatOffset + 1]};
         const glm::vec2 offset {floatVertices[floatOffset + 2],
                                 floatVertices[floatOffset + 3]};

         const glm::vec4 position =
            uniforms.uMapMatrix * glm::vec4 {p - mapScreenCoord, 0.0f, 1.0f} +
            uniforms.uMVPMatrix * glm::vec4 {offset, 0.0f, 0.0f};

         transformed.push_back(position.x);
         transformed.push_back(position.y);
         transformed.push_back(0.0f);
         transformed.push_back(floatVertices[floatOffset + 4]);
         transformed.push_back(floatVertices[floatOffset + 5]);
         transformed.push_back(floatVertices[floatOffset + 6]);
         transformed.push_back(floatVertices[floatOffset + 7]);
      }
   }

   return transformed;
}

} // namespace scwx::qt::render
