#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx::qt::map
{

class LightningLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(LightningLayer)

public:
   explicit LightningLayer(const std::shared_ptr<gl::GlContext>& context);
   ~LightningLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
