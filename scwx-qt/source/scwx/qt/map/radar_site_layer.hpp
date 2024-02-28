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
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(RadarSiteLayer)

public:
   explicit RadarSiteLayer(std::shared_ptr<MapContext> context);
   ~RadarSiteLayer();

   void Initialize() override final;
   void Render(const QMapLibre::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   bool RunMousePicking(
      const QMapLibre::CustomLayerRenderParameters& params,
      const QPointF&                                mouseLocalPos,
      const QPointF&                                mouseGlobalPos,
      const glm::vec2&                              mouseCoords,
      const common::Coordinate&                     mouseGeoCoords,
      std::shared_ptr<types::EventHandler>& eventHandler) override final;

signals:
   void RadarSiteSelected(const std::string& id);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
