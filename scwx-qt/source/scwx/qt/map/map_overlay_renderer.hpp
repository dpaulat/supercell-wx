#pragma once

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

   void Render(QRhiCommandBuffer*                                commandBuffer,
               QRhiTexture*                                      colorTexture,
               const std::vector<std::shared_ptr<GenericLayer>>& layers,
               const std::shared_ptr<MapContext>&                mapContext,
               const QMapLibre::CustomLayerRenderParameters&     params,
               const std::function<void(QRhiCommandBuffer*)>& imguiRender = {});

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
