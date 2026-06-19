#pragma once

#include <scwx/qt/settings/button_settings.hpp>
#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/types/radar_site_types.hpp>

#include <memory>

namespace scwx::qt::settings
{

class RadarSiteStatusPaletteSettings : public SettingsCategory
{
public:
   explicit RadarSiteStatusPaletteSettings(types::RadarSiteStatus status);
   ~RadarSiteStatusPaletteSettings() override;

   RadarSiteStatusPaletteSettings(const RadarSiteStatusPaletteSettings&) =
      delete;
   RadarSiteStatusPaletteSettings&
   operator=(const RadarSiteStatusPaletteSettings&) = delete;

   RadarSiteStatusPaletteSettings(RadarSiteStatusPaletteSettings&&) noexcept;
   RadarSiteStatusPaletteSettings&
   operator=(RadarSiteStatusPaletteSettings&&) noexcept;

   [[nodiscard]] ButtonSettings& button() const;

   friend bool operator==(const RadarSiteStatusPaletteSettings& lhs,
                          const RadarSiteStatusPaletteSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
