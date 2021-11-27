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

class RadarProductLayerImpl;

class RadarProductLayer : public GenericLayer
{
public:
   explicit RadarProductLayer(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl);
   ~RadarProductLayer();

   void Initialize() override final;
   void Render(const QMapbox::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

private:
   void UpdateColorTable();
   void UpdateSweep();

private:
   std::unique_ptr<RadarProductLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
