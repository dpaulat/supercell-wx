#pragma once

#include <scwx/qt/ui/settings/settings_page_widget.hpp>

#include <QWidget>

namespace scwx
{
namespace qt
{
namespace ui
{

class HotkeySettingsWidget : public SettingsPageWidget
{
   Q_OBJECT

public:
   explicit HotkeySettingsWidget(QWidget* parent = nullptr);
   ~HotkeySettingsWidget();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
