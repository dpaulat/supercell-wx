#pragma once

#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/qt/map/generic_layer.hpp>

#if defined(SCWX_RENDER_BACKEND_VULKAN)
class QRhiCommandBuffer;
#endif

namespace scwx::qt::map
{

class DrawLayer : public GenericLayer
{
   Q_DISABLE_COPY_MOVE(DrawLayer)

public:
   explicit DrawLayer(std::shared_ptr<render::RenderContext> renderContext,
                      const std::string&                     imGuiContextName);
   virtual ~DrawLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) override;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) override;
   void Deinitialize() override;

#if defined(SCWX_RENDER_BACKEND_VULKAN)
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
                   std::shared_ptr<types::EventHandler>& eventHandler) override;

protected:
   void AddDrawItem(const std::shared_ptr<gl::draw::DrawItem>& drawItem);
   void ImGuiFrameStart(const std::shared_ptr<MapContext>& mapContext);
   void ImGuiFrameEnd();
   void ImGuiInitialize(const std::shared_ptr<MapContext>& mapContext);
   void
   RenderWithoutImGui(const QMapLibre::CustomLayerRenderParameters& params);
   void ImGuiSelectContext();

#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void RenderWithoutImGuiVulkan(
      QRhiCommandBuffer*                            commandBuffer,
      render::RhiVulkanOverlayResources&            resources,
      const QMapLibre::CustomLayerRenderParameters& params);
   void ImGuiFrameStartVulkan(const std::shared_ptr<MapContext>& mapContext);
   void ImGuiFrameEndVulkan(QRhiCommandBuffer* commandBuffer);
#endif

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
