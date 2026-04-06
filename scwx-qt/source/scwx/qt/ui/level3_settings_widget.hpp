#pragma once

#include <scwx/qt/map/map_widget.hpp>

#include <optional>

namespace scwx::qt::ui
{

class Level3SettingsWidgetImpl;

class Level3SettingsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit Level3SettingsWidget(QWidget* parent = nullptr);
   ~Level3SettingsWidget();

   void UpdateThreshold(map::MapWidget* activeMap);

signals:
   void ThresholdChanged(std::optional<float> threshold);

private:
   std::shared_ptr<Level3SettingsWidgetImpl> p;
};

} // namespace scwx::qt::ui
