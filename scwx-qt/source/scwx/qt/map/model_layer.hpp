#pragma once

#include <scwx/qt/map/generic_layer.hpp>

namespace scwx::qt::map
{

class ModelLayer : public GenericLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ModelLayer)

public:
   explicit ModelLayer(std::shared_ptr<gl::GlContext> glContext);
   ~ModelLayer() override;

   void Initialize(const std::shared_ptr<MapContext>& mapContext) override;
   void Render(const std::shared_ptr<MapContext>&            mapContext,
               const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
