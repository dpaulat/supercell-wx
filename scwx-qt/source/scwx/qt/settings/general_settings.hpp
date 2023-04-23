#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_container.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

class GeneralSettingsImpl;

class GeneralSettings : public SettingsCategory
{
public:
   explicit GeneralSettings();
   ~GeneralSettings();

   GeneralSettings(const GeneralSettings&)            = delete;
   GeneralSettings& operator=(const GeneralSettings&) = delete;

   GeneralSettings(GeneralSettings&&) noexcept;
   GeneralSettings& operator=(GeneralSettings&&) noexcept;

   SettingsVariable<bool>&                       debug_enabled() const;
   SettingsVariable<std::string>&                default_radar_site() const;
   SettingsContainer<std::vector<std::int64_t>>& font_sizes() const;
   SettingsVariable<std::int64_t>&               grid_height() const;
   SettingsVariable<std::int64_t>&               grid_width() const;
   SettingsVariable<std::string>&                mapbox_api_key() const;
   SettingsVariable<bool>& update_notifications_enabled() const;

   friend bool operator==(const GeneralSettings& lhs,
                          const GeneralSettings& rhs);

private:
   std::unique_ptr<GeneralSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
