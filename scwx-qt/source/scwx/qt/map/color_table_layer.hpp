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

class ColorTableLayerImpl;

class ColorTableLayer : public GenericLayer
{
public:
   explicit ColorTableLayer(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl);
   ~ColorTableLayer();

   void Initialize() override final;
   void Render(const QMapbox::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

private:
   std::unique_ptr<ColorTableLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
