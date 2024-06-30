#pragma once

#include <scwx/awips/phenomenon.hpp>
#include <scwx/qt/map/draw_layer.hpp>

#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace qt
{
namespace map
{

class AlertLayer : public DrawLayer
{
   Q_DISABLE_COPY_MOVE(AlertLayer)

public:
   explicit AlertLayer(std::shared_ptr<MapContext> context,
                       scwx::awips::Phenomenon     phenomenon);
   ~AlertLayer();

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

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
