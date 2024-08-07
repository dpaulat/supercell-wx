#pragma once

#include <scwx/qt/types/text_event_key.hpp>

#include <QDockWidget>

namespace Ui
{
class AlertDockWidget;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class AlertDockWidgetImpl;

class AlertDockWidget : public QDockWidget
{
   Q_OBJECT

public:
   explicit AlertDockWidget(QWidget* parent = nullptr);
   ~AlertDockWidget();

protected:
   void showEvent(QShowEvent*) override;

signals:
   void MoveMap(double latitude, double longitude);

public slots:
   void HandleMapUpdate(double latitude, double longitude);
   void SelectAlert(const types::TextEventKey& key);

private:
   friend class AlertDockWidgetImpl;
   std::unique_ptr<AlertDockWidgetImpl> p;
   Ui::AlertDockWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
