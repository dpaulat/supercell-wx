#pragma once

#include <scwx/qt/map/draw_layer.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace map
{

class POILayer : public DrawLayer
{
   Q_OBJECT

public:
   explicit POILayer(const std::shared_ptr<MapContext>& context);
   ~POILayer();

   void Initialize() override final;
   void Render(const QMapLibre::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
