#pragma once

#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <qmaplibre.hpp>

class QRhi;
class QRhiCommandBuffer;

namespace scwx::qt::render
{

/**
 * Per-QRhi cache of compact geo-colored geometry renderers, used by draw items
 * with static (lat/long based) geometry. The vertex data is uploaded to the GPU
 * once, in chunks, and drawn directly with no per-frame CPU transform.
 *
 * Vertex layout (RhiGeoColoredGeometry::Layout::Compact):
 *   floats  (9): lat, long, x offset px, y offset px, r, g, b, a, perpendicular
 *   integers(3): threshold, start time, end time
 */
class RhiGeoColoredGeometryCache
{
public:
   /// Call whenever the vertex data changes
   void Invalidate();

   void Render(QRhiCommandBuffer*                            commandBuffer,
               RhiVulkanOverlayResources&                    resources,
               const QMapLibre::CustomLayerRenderParameters& params,
               bool                                          thresholded,
               std::chrono::system_clock::time_point         selectedTime,
               const std::vector<float>&                     floatVertices,
               const std::vector<std::int32_t>&              integerVertices,
               std::uint32_t                                 vertexCount);

   void Shutdown();

private:
   struct Entry
   {
      std::unique_ptr<RhiGeoColoredGeometry> renderer_ {};
      bool                                   geometryUploaded_ {false};
      std::uint64_t                          renderTargetGeneration_ {0};
   };

   std::unordered_map<QRhi*, Entry> entries_ {};
};

} // namespace scwx::qt::render
