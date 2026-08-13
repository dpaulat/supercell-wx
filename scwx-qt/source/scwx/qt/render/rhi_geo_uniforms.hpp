#pragma once

#include <qmaplibre.hpp>

#include <chrono>
#include <cstdint>

#include <glm/glm.hpp>

namespace scwx::qt::render
{

struct GeoUniforms
{
   alignas(16) glm::mat4 uMVPMatrix {};
   alignas(16) glm::mat4 uMapMatrix {};
   alignas(8) glm::vec2 uOriginLatLong {};
   alignas(4) float uMapDistance {0.0f};
   alignas(4) std::int32_t uSelectedTime {0};
};

[[nodiscard]] GeoUniforms
BuildGeoUniforms(const QMapLibre::CustomLayerRenderParameters& params,
                 bool                                          thresholded,
                 std::chrono::system_clock::time_point         selectedTime);

} // namespace scwx::qt::render
