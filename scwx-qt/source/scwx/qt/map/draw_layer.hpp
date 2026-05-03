#pragma once

#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/qt/map/generic_layer.hpp>

#include <string>

namespace scwx::qt::map
{

class DrawLayer : public GenericLayer
{
   Q_DISABLE_COPY_MOVE(DrawLayer)

public:
   explicit DrawLayer(std::shared_ptr<gl::GlContext> glContext,
                      const std::string&             imGuiContextName);
   virtual ~DrawLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) override;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) override;
   void Deinitialize() override;

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

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
