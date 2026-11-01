#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <qmaplibre.hpp>

#include <functional>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;

namespace scwx::qt::map
{

class GenericLayer;
class MapContext;

class MapOverlayRenderer
{
public:
   MapOverlayRenderer();
   ~MapOverlayRenderer();

   void Initialize(QRhi* rhi);
   void Shutdown();

   /**
    * Draws layers in order. When symbologyTexture is set (a premultiplied alpha
    * texture the size of colorTexture), it is composited after the first
    * symbologyLayerIndex layers, so basemap symbology (roads, labels) sits
    * above layers below it and under layers above it.
    *
    * Returns false if overlays were not composited (caller should keep the
    * pane dirty — otherwise drawings vanish until the next focus/paint). */
   bool Render(QRhiCommandBuffer*                                commandBuffer,
               QRhiTexture*                                      colorTexture,
               const std::vector<std::shared_ptr<GenericLayer>>& layers,
               const std::shared_ptr<MapContext>&                mapContext,
               const QMapLibre::CustomLayerRenderParameters&     params,
               const std::function<void(QRhiCommandBuffer*)>& imguiRender = {},
               QRhiTexture* symbologyTexture    = nullptr,
               std::size_t  symbologyLayerIndex = 0);

   /// Requests MSAA for overlay drawing (1 disables). Takes effect at the next
   /// frame; the effective count is limited by device support.
   void SetSampleCount(int sampleCount);
   [[nodiscard]] int GetSampleCount() const;

   [[nodiscard]] bool  EnsureRenderTarget(QRhiCommandBuffer* commandBuffer,
                                          QRhiTexture*       colorTexture);
   [[nodiscard]] void* GetNativeRenderPass() const;
   [[nodiscard]] std::uint64_t GetRenderTargetGeneration() const;

   [[nodiscard]] bool IsInitialized() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
