#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx::qt::map
{

class MarkerLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(MarkerLayer)

public:
   explicit MarkerLayer(const std::shared_ptr<render::RenderContext>& context);
   ~MarkerLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
