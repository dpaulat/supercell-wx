#include <scwx/qt/render/rhi_geo_colored_geometry_cache.hpp>
#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>

namespace scwx::qt::render
{

void RhiGeoColoredGeometryCache::Invalidate()
{
   for (auto& [rhi, entry] : entries_)
   {
      (void) rhi;
      entry.geometryUploaded_ = false;
   }
}

void RhiGeoColoredGeometryCache::Render(
   QRhiCommandBuffer*                            commandBuffer,
   RhiVulkanOverlayResources&                    resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   const bool                                    thresholded,
   const std::chrono::system_clock::time_point   selectedTime,
   const std::vector<float>&                     floatVertices,
   const std::vector<std::int32_t>&              integerVertices,
   const std::uint32_t                           vertexCount)
{
   if (resources.rhi == nullptr || resources.renderTarget == nullptr ||
       vertexCount == 0)
   {
      return;
   }

   Entry& entry = entries_[resources.rhi];

   if (entry.renderer_ != nullptr &&
       entry.renderTargetGeneration_ != resources.renderTargetGeneration)
   {
      entry.renderer_->Shutdown();
      entry.renderer_.reset();
   }
   if (entry.renderer_ == nullptr)
   {
      entry.renderer_               = std::make_unique<RhiGeoColoredGeometry>();
      entry.renderTargetGeneration_ = resources.renderTargetGeneration;
      entry.geometryUploaded_       = false;
   }
   if (!entry.renderer_->IsInitialized())
   {
      entry.renderer_->Initialize(resources.rhi,
                                  resources.renderTarget,
                                  commandBuffer,
                                  RhiGeoColoredGeometry::Layout::Compact);
      if (!entry.renderer_->IsInitialized())
      {
         return;
      }
   }

   const GeoUniforms uniforms = BuildGeoUniforms(
      params, thresholded, selectedTime, resources.layerOpacity);

   const bool uploadGeometry = !entry.geometryUploaded_;

   const bool moreWork = entry.renderer_->Render(commandBuffer,
                                                 uniforms,
                                                 floatVertices,
                                                 integerVertices,
                                                 vertexCount,
                                                 uploadGeometry,
                                                 resources.resourceBatch,
                                                 resources.phase);

   // Upload only starts in the upload phase; draw-only calls must not consume
   // the invalidation.
   if (uploadGeometry && OverlayShouldUpload(resources.phase))
   {
      entry.geometryUploaded_ = true;
   }
   if (moreWork)
   {
      resources.uploadPending = true;
   }
}

void RhiGeoColoredGeometryCache::Shutdown()
{
   for (auto& [rhi, entry] : entries_)
   {
      (void) rhi;
      if (entry.renderer_ != nullptr)
      {
         entry.renderer_->Shutdown();
      }
   }
   entries_.clear();
}

} // namespace scwx::qt::render
