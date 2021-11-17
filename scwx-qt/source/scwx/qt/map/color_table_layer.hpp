#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

#include <QMapboxGL>

namespace scwx
{
namespace qt
{
namespace map
{

class ColorTableLayerImpl;

class ColorTableLayer : public QObject, public QMapbox::CustomLayerHostInterface
{
   Q_OBJECT

public:
   explicit ColorTableLayer(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl);
   ~ColorTableLayer();

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

private:
   std::unique_ptr<ColorTableLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
