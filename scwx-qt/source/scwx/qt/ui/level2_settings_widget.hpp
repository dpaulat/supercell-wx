#pragma once

#include <scwx/qt/map/map_widget.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

class Level2SettingsWidgetImpl;

class Level2SettingsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit Level2SettingsWidget(QWidget* parent = nullptr);
   ~Level2SettingsWidget();

   bool event(QEvent* event) override;
   void showEvent(QShowEvent* event) override;

   void UpdateElevationSelection(float elevation);
   void UpdateSettings(map::MapWidget* activeMap);

signals:
   void ElevationSelected(float elevation);

private:
   std::shared_ptr<Level2SettingsWidgetImpl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
