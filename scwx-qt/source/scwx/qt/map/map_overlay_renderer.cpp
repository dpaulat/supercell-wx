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

#include <scwx/util/logger.hpp>

#include <algorithm>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::map_overlay_renderer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

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
   void DrawSymbology(QRhiCommandBuffer* commandBuffer, QRhiTexture* texture);
   void DrawBlit(QRhiCommandBuffer*           commandBuffer,
                 QRhiTexture*                 texture,
                 QRhiShaderResourceBindings*& srb,
                 QRhiTexture*&                srbTexture);
   [[nodiscard]] int EffectiveSampleCount() const;
   void              ReleaseMsaa();

   render::RhiColorTableOverlay   colorTableOverlay_ {};
   render::RhiRadarOverlay        radarOverlay_ {};
   render::RhiColoredGeometry     coloredGeometry_ {};
   render::RhiGeoColoredGeometry  radarGeoColoredGeometry_ {};
   render::RhiGeoColoredGeometry  geoColoredGeometry_ {};
   render::RhiTextureArrayOverlay textureArrayOverlay_ {};
   QRhiShaderResourceBindings*    blitSrb_ {nullptr};
   QRhiTexture*                   blitSrbTexture_ {nullptr};
   QRhiShaderResourceBindings*    baseSrb_ {nullptr};
   QRhiTexture*                   baseSrbTexture_ {nullptr};

   // Multisampled overlay target. The pane color texture is the resolve
   // target, so the basemap is first copied to baseCopy_ (a texture cannot be
   // sampled while it is a resolve attachment of the same pass).
   int                      requestedSamples_ {1};
   int                      activeSamples_ {1};
   QRhiRenderBuffer*        msaaBuffer_ {nullptr};
   QRhiTexture*             baseCopy_ {nullptr};
   QRhi*                    rhi_ {nullptr};
   QRhiTexture*             colorTexture_ {nullptr};
   QRhiTextureRenderTarget* preserveRenderTarget_ {nullptr};
   // Owned. Reused across color texture changes (resize) while the attachment
   // format and sample count match, so the native render pass handle, shared
   // pipelines, and overlay GPU state all stay valid.
   QRhiRenderPassDescriptor* renderPassDescriptor_ {nullptr};
   QRhiTexture::Format       rpdFormat_ {QRhiTexture::UnknownFormat};
   int                       rpdSamples_ {0};
   QSize                     pixelSize_ {};
   std::uint64_t             renderTargetGeneration_ {0};
   bool                      storeRetained_ {false};
};

void MapOverlayRenderer::Impl::DrawSymbology(QRhiCommandBuffer* commandBuffer,
                                             QRhiTexture*       texture)
{ DrawBlit(commandBuffer, texture, blitSrb_, blitSrbTexture_); }

void MapOverlayRenderer::Impl::DrawBlit(QRhiCommandBuffer* commandBuffer,
                                        QRhiTexture*       texture,
                                        QRhiShaderResourceBindings*& srb,
                                        QRhiTexture*&                srbTexture)
{
   if (texture == nullptr || preserveRenderTarget_ == nullptr ||
       texture->pixelSize() != pixelSize_)
   {
      return;
   }

   QRhiGraphicsPipeline* const pipeline =
      render::AcquireBlitPipeline(rhi_, preserveRenderTarget_);
   QRhiSampler* const sampler = render::AcquireNearestSampler(rhi_);
   if (pipeline == nullptr || sampler == nullptr)
   {
      return;
   }

   if (srb == nullptr || srbTexture != texture)
   {
      delete srb;
      srb = rhi_->newShaderResourceBindings();
      if (srb == nullptr)
      {
         return;
      }
      srb->setBindings({QRhiShaderResourceBinding::sampledTexture(
         0, QRhiShaderResourceBinding::FragmentStage, texture, sampler)});
      if (!srb->create())
      {
         delete srb;
         srb        = nullptr;
         srbTexture = nullptr;
         return;
      }
      srbTexture = texture;
   }

   commandBuffer->setGraphicsPipeline(pipeline);
   commandBuffer->setShaderResources(srb);
   commandBuffer->draw(3);
}

int MapOverlayRenderer::Impl::EffectiveSampleCount() const
{
#if defined(__APPLE__)
   // The Metal ImGui backend is not multisample aware
   return 1;
#else
   if (rhi_ == nullptr || requestedSamples_ <= 1)
   {
      return 1;
   }

   int best = 1;
   for (const int count : rhi_->supportedSampleCounts())
   {
      if (count <= requestedSamples_ && count > best)
      {
         best = count;
      }
   }
   return best;
#endif
}

void MapOverlayRenderer::Impl::ReleaseMsaa()
{
   delete msaaBuffer_;
   msaaBuffer_ = nullptr;
   delete baseCopy_;
   baseCopy_ = nullptr;
   delete baseSrb_;
   baseSrb_        = nullptr;
   baseSrbTexture_ = nullptr;
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

   const int samples = EffectiveSampleCount();

   const bool renderTargetChanged =
      preserveRenderTarget_ != nullptr &&
      (colorTexture_ != colorTexture || pixelSize_ != size ||
       activeSamples_ != samples);
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
      ReleaseMsaa();

      QRhiColorAttachment attachment {colorTexture};
      if (samples > 1)
      {
         msaaBuffer_ = rhi_->newRenderBuffer(
            QRhiRenderBuffer::Color, size, samples, {}, colorTexture->format());
         baseCopy_ = rhi_->newTexture(colorTexture->format(), size);
         if (msaaBuffer_ == nullptr || !msaaBuffer_->create() ||
             baseCopy_ == nullptr || !baseCopy_->create())
         {
            logger_->error("Failed to create MSAA overlay target");
            ReleaseMsaa();
            requestedSamples_ = 1;
            return false;
         }
         attachment = QRhiColorAttachment {msaaBuffer_};
         attachment.setResolveTexture(colorTexture);
      }

      QRhiTextureRenderTargetDescription desc {attachment};

      preserveRenderTarget_ = rhi_->newTextureRenderTarget(
         desc,
         samples > 1 ? QRhiTextureRenderTarget::Flags {} :
                       QRhiTextureRenderTarget::PreserveColorContents);
      if (preserveRenderTarget_ == nullptr)
      {
         return false;
      }

      const bool reuseRenderPass = renderPassDescriptor_ != nullptr &&
                                   rpdFormat_ == colorTexture->format() &&
                                   rpdSamples_ == samples;

      if (!reuseRenderPass)
      {
         delete renderPassDescriptor_;
         renderPassDescriptor_ =
            preserveRenderTarget_->newCompatibleRenderPassDescriptor();
         if (renderPassDescriptor_ == nullptr)
         {
            delete preserveRenderTarget_;
            preserveRenderTarget_ = nullptr;
            return false;
         }
         rpdFormat_  = colorTexture->format();
         rpdSamples_ = samples;
      }

      preserveRenderTarget_->setRenderPassDescriptor(renderPassDescriptor_);
      if (!preserveRenderTarget_->create())
      {
         delete preserveRenderTarget_;
         preserveRenderTarget_ = nullptr;
         return false;
      }

      colorTexture_  = colorTexture;
      pixelSize_     = size;
      activeSamples_ = samples;

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
   delete p->blitSrb_;
   p->blitSrb_        = nullptr;
   p->blitSrbTexture_ = nullptr;
   p->ReleaseMsaa();
   delete p->preserveRenderTarget_;
   p->preserveRenderTarget_ = nullptr;
   delete p->renderPassDescriptor_;
   p->renderPassDescriptor_ = nullptr;
   p->rpdFormat_            = QRhiTexture::UnknownFormat;
   p->rpdSamples_           = 0;
   p->colorTexture_         = nullptr;
   p->pixelSize_            = {};

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
   const std::function<void(QRhiCommandBuffer*)>&    imguiRender,
   QRhiTexture*                                      symbologyTexture,
   std::size_t                                       symbologyLayerIndex)
{
   if (p == nullptr || !p->EnsureReady(commandBuffer, colorTexture))
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

   for (const auto& layer : layers)
   {
      if (layer == nullptr)
      {
         continue;
      }

      resources.layerOpacity = layer->opacity();
      p->coloredGeometry_.SetLayerOpacity(resources.layerOpacity);
      layer->BindLayerState();
      layer->RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
      layer->ResetLayerState();
   }
   p->coloredGeometry_.UploadFrame(resources.resourceBatch);
   if (p->msaaBuffer_ != nullptr && p->baseCopy_ != nullptr)
   {
      // Snapshot the basemap; it is composited into the multisampled target
      // below (the color texture itself is only the resolve destination).
      resources.resourceBatch->copyTexture(p->baseCopy_, colorTexture);
   }
   commandBuffer->resourceUpdate(resources.resourceBatch);

   resources.phase         = render::RhiOverlayPhase::Draw;
   resources.resourceBatch = nullptr;

   const QRhiViewport viewport(0.0f,
                               0.0f,
                               static_cast<float>(p->pixelSize_.width()),
                               static_cast<float>(p->pixelSize_.height()));

   const bool multisampled = p->msaaBuffer_ != nullptr;

   commandBuffer->beginPass(p->preserveRenderTarget_,
                            multisampled ? QColor(Qt::transparent) : QColor(),
                            QRhiDepthStencilClearValue(),
                            nullptr,
                            QRhiCommandBuffer::ExternalContent);
   commandBuffer->setViewport(viewport);
   commandBuffer->setScissor(
      {0, 0, p->pixelSize_.width(), p->pixelSize_.height()});

   if (multisampled)
   {
      p->DrawBlit(commandBuffer, p->baseCopy_, p->baseSrb_, p->baseSrbTexture_);
   }

   bool symbologyDrawn = (symbologyTexture == nullptr);
   for (std::size_t i = 0; i < layers.size(); ++i)
   {
      if (!symbologyDrawn && i >= symbologyLayerIndex)
      {
         p->DrawSymbology(commandBuffer, symbologyTexture);
         symbologyDrawn = true;
      }

      const auto& layer = layers[i];
      if (layer == nullptr)
      {
         continue;
      }

      resources.layerOpacity = layer->opacity();
      p->coloredGeometry_.SetLayerOpacity(resources.layerOpacity);
      layer->BindLayerState();
      layer->RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
      layer->ResetLayerState();
   }

   if (!symbologyDrawn)
   {
      p->DrawSymbology(commandBuffer, symbologyTexture);
   }

   if (imguiRender)
   {
      imguiRender(commandBuffer);
   }

   commandBuffer->endPass();
   return true;
}

void MapOverlayRenderer::SetSampleCount(int sampleCount)
{
   if (p == nullptr)
   {
      p = std::make_unique<Impl>();
   }
   p->requestedSamples_ = std::max(1, sampleCount);
}

int MapOverlayRenderer::GetSampleCount() const
{ return p != nullptr ? p->activeSamples_ : 1; }

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
{ return p != nullptr ? p->renderTargetGeneration_ : 0; }

bool MapOverlayRenderer::IsInitialized() const
{ return p != nullptr && p->colorTableOverlay_.IsInitialized(); }

MapOverlayRenderer::MapOverlayRenderer()  = default;
MapOverlayRenderer::~MapOverlayRenderer() = default;

} // namespace scwx::qt::map
