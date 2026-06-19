#pragma once

#include <scwx/qt/ui/settings/settings_page_widget.hpp>

namespace scwx::qt::ui
{

class RadarSiteStatusPaletteSettingsWidget : public SettingsPageWidget
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(RadarSiteStatusPaletteSettingsWidget)

public:
   explicit RadarSiteStatusPaletteSettingsWidget(QWidget* parent = nullptr);
   ~RadarSiteStatusPaletteSettingsWidget() override;

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace scwx::qt::ui
