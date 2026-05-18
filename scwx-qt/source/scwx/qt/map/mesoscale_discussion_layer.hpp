#pragma once

#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>

#include <memory>

namespace scwx::qt::map
{

class MesoscaleDiscussionLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(MesoscaleDiscussionLayer)

public:
   explicit MesoscaleDiscussionLayer(std::shared_ptr<gl::GlContext> glContext);
   ~MesoscaleDiscussionLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) override;
   void Render(const std::shared_ptr<MapContext>&            mapContext,
               const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   bool
   RunMousePicking(const std::shared_ptr<MapContext>&            mapContext,
                   const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>& eventHandler) override;

signals:
   void MdSelected(int mdNumber);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
