#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class AlertLayerImpl;

class AlertLayer : public DrawLayer
{
public:
   explicit AlertLayer(std::shared_ptr<MapContext> context);
   ~AlertLayer();

   void Initialize() override final;
   void Render(const QMapLibreGL::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   void AddLayers(const std::string& before = {});

private:
   std::unique_ptr<AlertLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
