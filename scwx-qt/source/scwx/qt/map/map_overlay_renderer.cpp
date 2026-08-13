#include <scwx/qt/map/map_overlay_renderer.hpp>

#include <scwx/qt/map/generic_layer.hpp>
#include <scwx/qt/map/map_context.hpp>
#include <scwx/qt/render/rhi_colored_geometry.hpp>
#include <scwx/qt/render/rhi_color_table_overlay.hpp>
#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/qt/render/rhi_radar_overlay.hpp>
#include <scwx/qt/render/rhi_texture_array_overlay.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

namespace scwx::qt::map
{

namespace
{

void* NativeRenderPass(QRhiTextureRenderTarget* renderTarget)
{
   if (renderTarget == nullptr)
   {
      return nullptr;
   }

   QRhiRenderPassDescriptor* renderPassDescriptor =
      renderTarget->renderPassDescriptor();
   if (renderPassDescriptor == nullptr)
   {
      return nullptr;
   }

#if !defined(__APPLE__)
   const QRhiNativeHandles* nativeHandles =
      renderPassDescriptor->nativeHandles();
   if (nativeHandles != nullptr)
   {
      const auto* vkHandles =
         static_cast<const QRhiVulkanRenderPassNativeHandles*>(nativeHandles);
      if (vkHandles != nullptr && vkHandles->renderPass != nullptr)
      {
         return static_cast<void*>(vkHandles->renderPass);
      }
   }
#endif

   // Metal (and backends without a native pass handle): use the QRhi
   // descriptor pointer as a stable pipeline / ImGui identity key.
   return static_cast<void*>(renderPassDescriptor);
}

} // namespace

class MapOverlayRenderer::Impl
{
public:
   [[nodiscard]] bool EnsureReady(QRhiCommandBuffer* commandBuffer,
                                  QRhiTexture*       colorTexture);
   [[nodiscard]] bool EnsureBackdrop(QRhiTexture* colorTexture);
   void               ReleaseBackdrop();

   render::RhiColorTableOverlay   colorTableOverlay_ {};
   render::RhiRadarOverlay        radarOverlay_ {};
   render::RhiColoredGeometry     coloredGeometry_ {};
   render::RhiGeoColoredGeometry  radarGeoColoredGeometry_ {};
   render::RhiGeoColoredGeometry  geoColoredGeometry_ {};
   render::RhiTextureArrayOverlay textureArrayOverlay_ {};
   QRhi*                          rhi_ {nullptr};
   QRhiTexture*                   colorTexture_ {nullptr};
   QRhiTexture*                   backdropTexture_ {nullptr};
   QSize                          backdropSize_ {};
   QRhiTextureRenderTarget*       preserveRenderTarget_ {nullptr};
   QSize                          pixelSize_ {};
   std::uint64_t                  renderTargetGeneration_ {0};
   bool                           storeRetained_ {false};
};

void MapOverlayRenderer::Impl::ReleaseBackdrop()
{
   delete backdropTexture_;
   backdropTexture_ = nullptr;
   backdropSize_    = {};
}

bool MapOverlayRenderer::Impl::EnsureBackdrop(QRhiTexture* colorTexture)
{
   if (rhi_ == nullptr || colorTexture == nullptr)
   {
      return false;
   }

   const QSize size = colorTexture->pixelSize();
   if (size.isEmpty())
   {
      return false;
   }

   if (backdropTexture_ != nullptr && backdropSize_ == size &&
       backdropTexture_->format() == colorTexture->format())
   {
      return true;
   }

   ReleaseBackdrop();
   backdropTexture_ = rhi_->newTexture(colorTexture->format(), size);
   if (backdropTexture_ == nullptr || !backdropTexture_->create())
   {
      ReleaseBackdrop();
      return false;
   }
   backdropSize_ = size;
   return true;
}

bool MapOverlayRenderer::Impl::EnsureReady(QRhiCommandBuffer* commandBuffer,
                                           QRhiTexture*       colorTexture)
{
   if (rhi_ == nullptr || commandBuffer == nullptr || colorTexture == nullptr)
   {
      return false;
   }

   const QSize size = colorTexture->pixelSize();
   if (size.isEmpty())
   {
      return false;
   }

   const bool renderTargetChanged =
      preserveRenderTarget_ != nullptr &&
      (colorTexture_ != colorTexture || pixelSize_ != size);
   const void* oldNativeRenderPass =
      renderTargetChanged ? NativeRenderPass(preserveRenderTarget_) : nullptr;

   if (renderTargetChanged)
   {
      delete preserveRenderTarget_;
      preserveRenderTarget_ = nullptr;
      colorTexture_         = nullptr;
      pixelSize_            = {};
      ReleaseBackdrop();
   }

   if (preserveRenderTarget_ == nullptr)
   {
      QRhiTextureRenderTargetDescription desc {
         QRhiColorAttachment {colorTexture}};

      preserveRenderTarget_ = rhi_->newTextureRenderTarget(
         desc, QRhiTextureRenderTarget::PreserveColorContents);
      if (preserveRenderTarget_ == nullptr)
      {
         return false;
      }

      QRhiRenderPassDescriptor* renderPassDescriptor =
         preserveRenderTarget_->newCompatibleRenderPassDescriptor();
      if (renderPassDescriptor == nullptr)
      {
         delete preserveRenderTarget_;
         preserveRenderTarget_ = nullptr;
         return false;
      }

      preserveRenderTarget_->setRenderPassDescriptor(renderPassDescriptor);
      if (!preserveRenderTarget_->create())
      {
         delete preserveRenderTarget_;
         preserveRenderTarget_ = nullptr;
         return false;
      }

      colorTexture_ = colorTexture;
      pixelSize_    = size;

      const void* newNativeRenderPass = NativeRenderPass(preserveRenderTarget_);
      if (oldNativeRenderPass == nullptr ||
          oldNativeRenderPass != newNativeRenderPass)
      {
         // Drop this pane's overlay objects so they rebind to new pass.
         // Shared pipelines for the old pass key stay until unused; other
         // panes may still hold them. New pass gets a new store key.
         colorTableOverlay_.Shutdown();
         radarOverlay_.Shutdown();
         coloredGeometry_.Shutdown();
         textureArrayOverlay_.Shutdown();
         ++renderTargetGeneration_;
      }
   }

   if (!colorTableOverlay_.IsInitialized())
   {
      colorTableOverlay_.Initialize(rhi_, preserveRenderTarget_, commandBuffer);
   }
   if (!coloredGeometry_.IsInitialized())
   {
      coloredGeometry_.Initialize(rhi_, preserveRenderTarget_, commandBuffer);
   }
   if (!radarOverlay_.IsInitialized())
   {
      radarOverlay_.Initialize(rhi_, preserveRenderTarget_, commandBuffer);
   }
   if (!textureArrayOverlay_.IsInitialized())
   {
      textureArrayOverlay_.Initialize(
         rhi_, preserveRenderTarget_, commandBuffer);
   }

   // Colored geometry alone is enough to keep annotations/lines visible.
   // Requiring every overlay subsystem used to abort the whole pass after a
   // basemap copy — inactive panes then showed a clean map with drawings gone.
   return coloredGeometry_.IsInitialized();
}

void MapOverlayRenderer::Initialize(QRhi* rhi)
{
   if (p == nullptr)
   {
      p = std::make_unique<Impl>();
   }

   if (p->rhi_ != rhi)
   {
      if (p->storeRetained_ && p->rhi_ != nullptr)
      {
         render::ReleaseOverlayGpuStore(p->rhi_);
         p->storeRetained_ = false;
      }
      p->rhi_ = rhi;
   }

   if (p->rhi_ != nullptr && !p->storeRetained_)
   {
      render::RetainOverlayGpuStore(p->rhi_);
      p->storeRetained_ = true;
   }
}

void MapOverlayRenderer::Shutdown()
{
   if (p == nullptr)
   {
      return;
   }

   p->colorTableOverlay_.Shutdown();
   p->radarOverlay_.Shutdown();
   p->coloredGeometry_.Shutdown();
   p->radarGeoColoredGeometry_.Shutdown();
   p->geoColoredGeometry_.Shutdown();
   p->textureArrayOverlay_.Shutdown();
   delete p->preserveRenderTarget_;
   p->preserveRenderTarget_ = nullptr;
   p->colorTexture_         = nullptr;
   p->pixelSize_            = {};
   p->ReleaseBackdrop();

   if (p->storeRetained_ && p->rhi_ != nullptr)
   {
      render::ReleaseOverlayGpuStore(p->rhi_);
      p->storeRetained_ = false;
   }
   p->rhi_ = nullptr;
}

bool MapOverlayRenderer::Render(
   QRhiCommandBuffer*                                commandBuffer,
   QRhiTexture*                                      colorTexture,
   const std::vector<std::shared_ptr<GenericLayer>>& layers,
   const std::shared_ptr<MapContext>&                mapContext,
   const QMapLibre::CustomLayerRenderParameters&     params,
   const std::function<void(QRhiCommandBuffer*)>&    imguiRender)
{
   if (p == nullptr || !p->EnsureReady(commandBuffer, colorTexture))
   {
      return false;
   }

   if (!p->EnsureBackdrop(colorTexture))
   {
      return false;
   }

   render::RhiVulkanOverlayResources resources {p->colorTableOverlay_,
                                                p->radarOverlay_,
                                                p->coloredGeometry_,
                                                p->radarGeoColoredGeometry_,
                                                p->geoColoredGeometry_,
                                                p->textureArrayOverlay_,
                                                p->rhi_,
                                                p->preserveRenderTarget_,
                                                p->renderTargetGeneration_};

   resources.phase         = render::RhiOverlayPhase::Upload;
   resources.resourceBatch = p->rhi_->nextResourceUpdateBatch();
   if (resources.resourceBatch == nullptr)
   {
      // Inside a pass or out of batches — drawing would underrun empty cmds
      // and leave a basemap-only frame (annotations "vanish" until refocus).
      return false;
   }

   // Shared coloredGeometry accumulates every drawer upload this frame, then
   // Draw consumes recorded vertex ranges (avoids last-upload-wins clobber).
   p->coloredGeometry_.BeginFrame();

   // Snapshot destination before the overlay pass so color.frag can mix
   // translucent geometry without relying on broken SrcAlpha blending.
   resources.resourceBatch->copyTexture(p->backdropTexture_, colorTexture);

   for (const auto& layer : layers)
   {
      if (layer == nullptr)
      {
         continue;
      }

      layer->RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
   }
   p->coloredGeometry_.UploadFrame(resources.resourceBatch);
   commandBuffer->resourceUpdate(resources.resourceBatch);

   QRhiSampler* const backdropSampler = render::AcquireNearestSampler(p->rhi_);
   p->coloredGeometry_.SetBackdrop(p->backdropTexture_, backdropSampler);

   resources.phase         = render::RhiOverlayPhase::Draw;
   resources.resourceBatch = nullptr;

   const QRhiViewport viewport(0.0f,
                               0.0f,
                               static_cast<float>(p->pixelSize_.width()),
                               static_cast<float>(p->pixelSize_.height()));

   commandBuffer->beginPass(p->preserveRenderTarget_,
                            QColor(),
                            QRhiDepthStencilClearValue(),
                            nullptr,
                            QRhiCommandBuffer::ExternalContent);
   commandBuffer->setViewport(viewport);
   commandBuffer->setScissor(
      {0, 0, p->pixelSize_.width(), p->pixelSize_.height()});

   for (const auto& layer : layers)
   {
      if (layer == nullptr)
      {
         continue;
      }

      layer->RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
   }

   if (imguiRender)
   {
      imguiRender(commandBuffer);
   }

   commandBuffer->endPass();
   return true;
}

bool MapOverlayRenderer::EnsureRenderTarget(QRhiCommandBuffer* commandBuffer,
                                            QRhiTexture*       colorTexture)
{
   if (p == nullptr)
   {
      return false;
   }

   return p->EnsureReady(commandBuffer, colorTexture);
}

void* MapOverlayRenderer::GetNativeRenderPass() const
{
   if (p == nullptr || p->preserveRenderTarget_ == nullptr)
   {
      return nullptr;
   }

   return NativeRenderPass(p->preserveRenderTarget_);
}

std::uint64_t MapOverlayRenderer::GetRenderTargetGeneration() const
{
   return p != nullptr ? p->renderTargetGeneration_ : 0;
}

bool MapOverlayRenderer::IsInitialized() const
{
   return p != nullptr && p->colorTableOverlay_.IsInitialized();
}

MapOverlayRenderer::MapOverlayRenderer()  = default;
MapOverlayRenderer::~MapOverlayRenderer() = default;

} // namespace scwx::qt::map
