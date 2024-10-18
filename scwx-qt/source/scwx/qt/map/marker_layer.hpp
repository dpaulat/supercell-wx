#pragma once

#include <scwx/qt/map/draw_layer.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace map
{

class MarkerLayer : public DrawLayer
{
   Q_OBJECT

public:
   explicit MarkerLayer(const std::shared_ptr<MapContext>& context);
   ~MarkerLayer();

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
