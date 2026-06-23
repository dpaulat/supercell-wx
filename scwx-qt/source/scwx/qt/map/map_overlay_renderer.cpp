#include <scwx/qt/map/map_overlay_renderer.hpp>

#include <scwx/qt/map/generic_layer.hpp>
#include <scwx/qt/map/map_context.hpp>
#include <scwx/qt/render/rhi_colored_geometry.hpp>
#include <scwx/qt/render/rhi_color_table_overlay.hpp>
#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
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

   const QRhiNativeHandles* nativeHandles =
      renderPassDescriptor->nativeHandles();
   if (nativeHandles == nullptr)
   {
      return nullptr;
   }

   const auto* vkHandles =
      static_cast<const QRhiVulkanRenderPassNativeHandles*>(nativeHandles);
   return static_cast<void*>(vkHandles->renderPass);
}

} // namespace

class MapOverlayRenderer::Impl
{
public:
   [[nodiscard]] bool EnsureReady(QRhiCommandBuffer* commandBuffer,
                                  QRhiTexture*       colorTexture);

   render::RhiColorTableOverlay   colorTableOverlay_ {};
   render::RhiRadarOverlay        radarOverlay_ {};
   render::RhiColoredGeometry     coloredGeometry_ {};
   render::RhiGeoColoredGeometry  radarGeoColoredGeometry_ {};
   render::RhiGeoColoredGeometry  geoColoredGeometry_ {};
   render::RhiTextureArrayOverlay textureArrayOverlay_ {};
   QRhi*                          rhi_ {nullptr};
   QRhiTexture*                   colorTexture_ {nullptr};
   QRhiTextureRenderTarget*       preserveRenderTarget_ {nullptr};
   QSize                          pixelSize_ {};
   std::uint64_t                  renderTargetGeneration_ {0};
};

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
         colorTableOverlay_.Shutdown();
         radarOverlay_.Shutdown();
         coloredGeometry_.Shutdown();
         radarGeoColoredGeometry_.Shutdown();
         geoColoredGeometry_.Shutdown();
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
   if (!radarGeoColoredGeometry_.IsInitialized())
   {
      radarGeoColoredGeometry_.Initialize(
         rhi_, preserveRenderTarget_, commandBuffer);
   }
   if (!geoColoredGeometry_.IsInitialized())
   {
      geoColoredGeometry_.Initialize(
         rhi_, preserveRenderTarget_, commandBuffer);
   }
   if (!textureArrayOverlay_.IsInitialized())
   {
      textureArrayOverlay_.Initialize(
         rhi_, preserveRenderTarget_, commandBuffer);
   }

   return colorTableOverlay_.IsInitialized() && radarOverlay_.IsInitialized() &&
          coloredGeometry_.IsInitialized() &&
          radarGeoColoredGeometry_.IsInitialized() &&
          geoColoredGeometry_.IsInitialized() &&
          textureArrayOverlay_.IsInitialized();
}

void MapOverlayRenderer::Initialize(QRhi* rhi)
{
   if (p == nullptr)
   {
      p = std::make_unique<Impl>();
   }

   p->rhi_ = rhi;
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
   p->rhi_                  = nullptr;
}

void MapOverlayRenderer::Render(
   QRhiCommandBuffer*                                commandBuffer,
   QRhiTexture*                                      colorTexture,
   const std::vector<std::shared_ptr<GenericLayer>>& layers,
   const std::shared_ptr<MapContext>&                mapContext,
   const QMapLibre::CustomLayerRenderParameters&     params,
   const std::function<void(QRhiCommandBuffer*)>&    imguiRender)
{
   if (p == nullptr || !p->EnsureReady(commandBuffer, colorTexture))
   {
      return;
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
