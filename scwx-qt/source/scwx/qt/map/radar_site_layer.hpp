#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx::qt::map
{

class RadarSiteLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(RadarSiteLayer)

public:
   explicit RadarSiteLayer(const std::shared_ptr<render::RenderContext>& renderContext);
   ~RadarSiteLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void RenderVulkanImGui(
      const std::shared_ptr<MapContext>&            mapContext,
      const QMapLibre::CustomLayerRenderParameters& params);
   void RenderVulkanOverlay(
      QRhiCommandBuffer*                            commandBuffer,
      render::RhiVulkanOverlayResources&            resources,
      const std::shared_ptr<MapContext>&            mapContext,
      const QMapLibre::CustomLayerRenderParameters& params) override;
#endif

   bool
   RunMousePicking(const std::shared_ptr<MapContext>&            mapContext,
                   const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>& eventHandler) final;

signals:
   void RadarSiteSelected(const std::string& id);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
