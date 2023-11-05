#pragma once

#include <QFrame>

namespace Ui
{
class PlacefileSettingsWidget;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class PlacefileSettingsWidgetImpl;

class PlacefileSettingsWidget : public QFrame
{
   Q_OBJECT

public:
   explicit PlacefileSettingsWidget(QWidget* parent = nullptr);
   ~PlacefileSettingsWidget();

private:
   friend class PlacefileSettingsWidgetImpl;
   std::unique_ptr<PlacefileSettingsWidgetImpl> p;
   Ui::PlacefileSettingsWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
