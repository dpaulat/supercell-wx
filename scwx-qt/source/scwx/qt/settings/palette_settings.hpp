#pragma once

#include <scwx/qt/settings/alert_palette_settings.hpp>
#include <scwx/qt/settings/radar_site_status_palette_settings.hpp>
#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/types/radar_site_types.hpp>
#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/awips/phenomenon.hpp>

#include <memory>
#include <string>

namespace scwx::qt::settings
{

class PaletteSettings : public SettingsCategory
{
public:
   explicit PaletteSettings();
   ~PaletteSettings() override;

   PaletteSettings(const PaletteSettings&)            = delete;
   PaletteSettings& operator=(const PaletteSettings&) = delete;

   PaletteSettings(PaletteSettings&&) noexcept;
   PaletteSettings& operator=(PaletteSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<std::string>&
   palette(const std::string& name) const;
   [[nodiscard]] SettingsVariable<std::string>&
   alert_color(awips::Phenomenon phenomenon, bool active) const;
   AlertPaletteSettings& alert_palette(awips::Phenomenon);
   RadarSiteStatusPaletteSettings&
      radar_site_status_palette(types::RadarSiteStatus);

   static const std::vector<awips::Phenomenon>& alert_phenomena();

   static PaletteSettings& Instance();

   friend bool operator==(const PaletteSettings& lhs,
                          const PaletteSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
