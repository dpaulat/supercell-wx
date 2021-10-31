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

class RadarProductLayerImpl;

class RadarProductLayer :
    public QObject,
    public QMapbox::CustomLayerHostInterface
{
   Q_OBJECT

public:
   explicit RadarProductLayer(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl);
   ~RadarProductLayer();

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

private:
   void UpdateColorTable();
   void UpdateSweep();

private slots:
   void UpdateColorTableNextFrame();
   void UpdateSweepNextFrame();

private:
   std::unique_ptr<RadarProductLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
