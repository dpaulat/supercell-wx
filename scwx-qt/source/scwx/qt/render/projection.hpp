#pragma once

#include <qmaplibre.hpp>

#include <chrono>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace scwx::qt::render
{

[[nodiscard]] glm::mat4
OrthoMapProjection(const QMapLibre::CustomLayerRenderParameters& params);

[[nodiscard]] std::vector<float>
TransformMapColorVertices(const std::vector<float>&        floatVertices,
                          const std::vector<std::int32_t>& integerVertices,
                          const QMapLibre::CustomLayerRenderParameters& params,
                          bool                                  thresholded,
                          std::chrono::system_clock::time_point selectedTime);

} // namespace scwx::qt::render
