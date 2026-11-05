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
   alignas(4) float uOpacity {1.0f};
   alignas(4) float _pad0 {0.0f};
   alignas(8) glm::vec2 _pad1 {};
};

static_assert(sizeof(GeoUniforms) == 160);

[[nodiscard]] GeoUniforms
BuildGeoUniforms(const QMapLibre::CustomLayerRenderParameters& params,
                 bool                                          thresholded,
                 std::chrono::system_clock::time_point         selectedTime,
                 float                                         opacity = 1.0f);

} // namespace scwx::qt::render
