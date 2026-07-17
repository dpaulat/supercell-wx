#include <scwx/qt/map/map_rhi_renderer.hpp>

#include <scwx/util/logger.hpp>

#include <QMapLibre/Map>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <QtGui/qwindow.h>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::map_rhi_renderer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

void MapRhiRenderer::InitializeMapRenderer(QRhi*           rhi,
                                           QWindow*        window,
                                           QMapLibre::Map* map)
{
   if (rhi == nullptr || map == nullptr)
   {
      return;
   }

   map->destroyRenderer();
   initialized_ = false;

#if !defined(__APPLE__)
   if (rhi->backend() == QRhi::Vulkan)
   {
      const QRhiNativeHandles* nativeHandles = rhi->nativeHandles();
      if (nativeHandles != nullptr)
      {
         const auto* vkHandles =
            static_cast<const QRhiVulkanNativeHandles*>(nativeHandles);
         if (vkHandles != nullptr && vkHandles->physDev != nullptr &&
             vkHandles->dev != nullptr && window != nullptr &&
             vkHandles->inst != nullptr)
         {
            if (window->vulkanInstance() == nullptr)
            {
               window->setVulkanInstance(vkHandles->inst);
            }

            map->createRendererWithQtVulkanDevice(window,
                                                  vkHandles->physDev,
                                                  vkHandles->dev,
                                                  vkHandles->gfxQueueFamilyIdx);
            initialized_ = true;
            logger_->debug("MapLibre Vulkan renderer initialized");
            return;
         }
      }
   }
#else
   (void) window;
#endif

   // Metal (and fallback): MapLibre creates its own device/context and
   // receives the QRhi color texture each frame via setExternalDrawable.
   map->createRenderer(nullptr);
   initialized_ = true;
   logger_->debug("MapLibre renderer initialized (backend={})",
                  static_cast<int>(rhi->backend()));
}

void MapRhiRenderer::RenderMap(QRhiTexture* colorTexture, QMapLibre::Map* map)
{
   if (colorTexture == nullptr || map == nullptr || !initialized_)
   {
      return;
   }

   const QRhiTexture::NativeTexture nativeTexture =
      colorTexture->nativeTexture();

   // Vulkan: VkImage*; Metal: MTLTexture* — MapLibre interprets by backend.
   auto* nativePtr = reinterpret_cast<void*>(nativeTexture.object); // NOLINT
   if (nativePtr != nullptr)
   {
      map->setExternalDrawable(nativePtr, colorTexture->pixelSize());
   }

   map->render();
}

void MapRhiRenderer::CopyColorTexture(QRhi*              rhi,
                                      QRhiCommandBuffer* commandBuffer,
                                      QRhiTexture*       destination,
                                      QRhiTexture*       source)
{
   if (rhi == nullptr || commandBuffer == nullptr || destination == nullptr ||
       source == nullptr)
   {
      return;
   }

   if (destination->pixelSize() != source->pixelSize() ||
       destination->format() != source->format())
   {
      return;
   }

   QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
   if (batch == nullptr)
   {
      return;
   }

   batch->copyTexture(destination, source);
   commandBuffer->resourceUpdate(batch);
}

void MapRhiRenderer::ReleaseMapRenderer(QMapLibre::Map* map)
{
   if (map == nullptr)
   {
      return;
   }

   map->destroyRenderer();
   initialized_ = false;
}

bool MapRhiRenderer::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::map
