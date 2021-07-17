#pragma once

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

class MapWidget : public QOpenGLWidget
{
   Q_OBJECT

public:
   MapWidget(const QMapboxGLSettings&);
   ~MapWidget();

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

   QPointF lastPos_;

   QMapboxGLSettings          settings_;
   std::shared_ptr<QMapboxGL> map_;

   uint64_t frameDraws_ = 0;

private slots:
   void mapChanged(QMapboxGL::MapChange);
};

} // namespace qt
} // namespace scwx
