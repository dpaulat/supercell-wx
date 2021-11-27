#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/map/generic_layer.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class OverlayLayerImpl;

class OverlayLayer : public GenericLayer
{
public:
   explicit OverlayLayer(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl);
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
