#pragma once

#include <scwx/qt/map/generic_layer.hpp>

namespace scwx::qt::map
{

class ColorTableLayer : public GenericLayer
{
   Q_DISABLE_COPY_MOVE(ColorTableLayer)

public:
   explicit ColorTableLayer(std::shared_ptr<render::RenderContext> renderContext);
   ~ColorTableLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void RenderVulkanOverlay(
      QRhiCommandBuffer*                            commandBuffer,
      render::RhiVulkanOverlayResources&            resources,
      const std::shared_ptr<MapContext>&            mapContext,
      const QMapLibre::CustomLayerRenderParameters& params) final;
#endif
   void Deinitialize() final;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
