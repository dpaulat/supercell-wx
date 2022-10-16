#pragma once

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

public slots:
   void HandleMapUpdate(double latitude, double longitude);

private:
   friend class AlertDockWidgetImpl;
   std::unique_ptr<AlertDockWidgetImpl> p;
   Ui::AlertDockWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
