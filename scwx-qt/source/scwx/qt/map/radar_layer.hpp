#pragma once

#include <scwx/qt/util/gl.hpp>
#include <scwx/qt/view/radar_view.hpp>

#include <QMapboxGL>

namespace scwx
{
namespace qt
{

class RadarLayerImpl;

class RadarLayer : public QObject, public QMapbox::CustomLayerHostInterface
{
   Q_OBJECT

public:
   explicit RadarLayer(std::shared_ptr<view::RadarView> radarView,
                       OpenGLFunctions&                 gl);
   ~RadarLayer();

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

   void UpdateColorTable();
   void UpdatePlot();

public slots:
   void ReceiveColorTableUpdate();
   void ReceivePlotUpdate();

private:
   std::unique_ptr<RadarLayerImpl> p;
};

} // namespace qt
} // namespace scwx
