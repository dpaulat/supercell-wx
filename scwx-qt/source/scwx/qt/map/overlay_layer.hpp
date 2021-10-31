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

class OverlayLayerImpl;

class OverlayLayer : public QObject, public QMapbox::CustomLayerHostInterface
{
   Q_OBJECT

public:
   explicit OverlayLayer(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl);
   ~OverlayLayer();

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

public slots:
   void ReceivePlotUpdate();

private:
   std::unique_ptr<OverlayLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
