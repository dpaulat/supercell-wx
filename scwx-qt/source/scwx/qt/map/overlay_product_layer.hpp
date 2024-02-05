#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class OverlayProductLayer : public DrawLayer
{
public:
   explicit OverlayProductLayer(std::shared_ptr<MapContext> context);
   ~OverlayProductLayer();

   void Initialize() override final;
   void Render(const QMapLibreGL::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   bool RunMousePicking(
      const QMapLibreGL::CustomLayerRenderParameters& params,
      const QPointF&                                  mouseLocalPos,
      const QPointF&                                  mouseGlobalPos,
      const glm::vec2&                                mouseCoords,
      const common::Coordinate&                       mouseGeoCoords,
      std::shared_ptr<types::EventHandler>& eventHandler) override final;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
