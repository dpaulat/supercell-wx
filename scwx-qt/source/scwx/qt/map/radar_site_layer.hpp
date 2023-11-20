#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class RadarSiteLayer : public DrawLayer
{
public:
   explicit RadarSiteLayer(std::shared_ptr<MapContext> context);
   ~RadarSiteLayer();

   void Initialize() override final;
   void Render(const QMapLibreGL::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   bool RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                        const QPointF&   mouseLocalPos,
                        const QPointF&   mouseGlobalPos,
                        const glm::vec2& mouseCoords) override final;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
