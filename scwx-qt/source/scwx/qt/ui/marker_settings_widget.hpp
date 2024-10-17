#pragma once

#include <QFrame>

namespace Ui
{
class MarkerSettingsWidget;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class MarkerSettingsWidgetImpl;

class MarkerSettingsWidget : public QFrame
{
   Q_OBJECT

public:
   explicit MarkerSettingsWidget(QWidget* parent = nullptr);
   ~MarkerSettingsWidget();

private:
   friend class MarkerSettingsWidgetImpl;
   std::unique_ptr<MarkerSettingsWidgetImpl> p;
   Ui::MarkerSettingsWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
