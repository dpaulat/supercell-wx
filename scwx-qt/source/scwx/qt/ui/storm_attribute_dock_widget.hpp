#pragma once

#include <memory>

#include <scwx/common/geographic.hpp>

#include <QDockWidget>

namespace scwx::qt::manager
{
class RadarProductManager;
} // namespace scwx::qt::manager

namespace scwx::qt::view
{
class OverlayProductView;
} // namespace scwx::qt::view

namespace scwx
{
namespace qt
{
namespace ui
{

class StormAttributeDockWidgetImpl;

class StormAttributeDockWidget : public QDockWidget
{
   Q_OBJECT

public:
   explicit StormAttributeDockWidget(QWidget* parent = nullptr);
   ~StormAttributeDockWidget();

   void SetRadarProductManager(
      const std::shared_ptr<manager::RadarProductManager>& radarProductManager);
   void SetOverlayProductView(
      const std::shared_ptr<view::OverlayProductView>& overlayProductView);

protected:
   void showEvent(QShowEvent* event) override;

signals:
   void ZoomToStorm(double latitude, double longitude);

public slots:
   void HandleDataUpdate();
   void HandleMapUpdate(double latitude, double longitude);

private:
   friend class StormAttributeDockWidgetImpl;
   std::unique_ptr<StormAttributeDockWidgetImpl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
