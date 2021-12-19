#pragma once

#include <scwx/qt/map/draw_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class OverlayLayerImpl;

class OverlayLayer : public DrawLayer
{
public:
   explicit OverlayLayer(std::shared_ptr<MapContext> context);
   ~OverlayLayer();

   void Initialize() override final;
   void Render(const QMapbox::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

public slots:
   void UpdateSweepTimeNextFrame();

private:
   std::unique_ptr<OverlayLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
