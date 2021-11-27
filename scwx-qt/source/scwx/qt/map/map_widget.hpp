#pragma once

#include <scwx/common/products.hpp>

#include <memory>

#include <QMapboxGL>

#include <QOpenGLWidget>
#include <QPropertyAnimation>
#include <QtGlobal>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace scwx
{
namespace qt
{
namespace map
{

class MapWidgetImpl;

class MapWidget : public QOpenGLWidget
{
   Q_OBJECT

public:
   explicit MapWidget(const QMapboxGLSettings&);
   ~MapWidget();

   float              GetElevation() const;
   std::vector<float> GetElevationCuts() const;
   void               SelectElevation(float elevation);
   void               SelectRadarProduct(common::Level2Product product);
   void               SetActive(bool isActive);
   void               SetMapParameters(double latitude,
                                       double longitude,
                                       double zoom,
                                       double bearing,
                                       double pitch);

private:
   void  changeStyle();
   qreal pixelRatio();

   // QWidget implementation.
   void keyPressEvent(QKeyEvent* ev) override final;
   void mousePressEvent(QMouseEvent* ev) override final;
   void mouseMoveEvent(QMouseEvent* ev) override final;
   void wheelEvent(QWheelEvent* ev) override final;

   // QOpenGLWidget implementation.
   void initializeGL() override final;
   void paintGL() override final;

   void AddLayers();

   std::unique_ptr<MapWidgetImpl> p;

private slots:
   void mapChanged(QMapboxGL::MapChange);

signals:
   void MapParametersChanged(double latitude,
                             double longitude,
                             double zoom,
                             double bearing,
                             double pitch);
   void RadarSweepUpdated();
};

} // namespace map
} // namespace qt
} // namespace scwx
