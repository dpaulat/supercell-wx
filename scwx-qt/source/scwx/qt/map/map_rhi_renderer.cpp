#include <scwx/qt/map/map_rhi_renderer.hpp>

#include <QMapLibre/Map>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <QtGui/qwindow.h>

namespace scwx::qt::map
{

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
            return;
         }
      }
   }

   map->createRenderer(nullptr);
}

void MapRhiRenderer::RenderMap(QRhiTexture* colorTexture, QMapLibre::Map* map)
{
   if (colorTexture == nullptr || map == nullptr || !initialized_)
   {
      return;
   }

   const QRhiTexture::NativeTexture nativeTexture =
      colorTexture->nativeTexture();

   auto* vulkanImagePtr =
      reinterpret_cast<void*>(nativeTexture.object); // NOLINT
   if (vulkanImagePtr != nullptr)
   {
      map->setExternalDrawable(vulkanImagePtr, colorTexture->pixelSize());
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
