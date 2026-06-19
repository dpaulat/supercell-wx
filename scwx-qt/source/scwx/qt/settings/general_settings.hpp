#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_container.hpp>

#include <memory>
#include <string>

namespace scwx::qt::settings
{

class GeneralSettings : public SettingsCategory
{
public:
   explicit GeneralSettings();
   ~GeneralSettings() override;

   GeneralSettings(const GeneralSettings&)            = delete;
   GeneralSettings& operator=(const GeneralSettings&) = delete;

   GeneralSettings(GeneralSettings&&) noexcept;
   GeneralSettings& operator=(GeneralSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<bool>& anti_aliasing_enabled() const;
   [[nodiscard]] SettingsVariable<bool>& auto_navigate_to_wsr88d_only() const;
   [[nodiscard]] SettingsVariable<bool>& center_on_radar_selection() const;
   [[nodiscard]] SettingsVariable<std::string>& clock_format() const;
   [[nodiscard]] SettingsVariable<std::string>& custom_style_draw_layer() const;
   [[nodiscard]] SettingsVariable<std::string>& custom_style_url() const;
   [[nodiscard]] SettingsVariable<bool>&        debug_enabled() const;
   [[nodiscard]] SettingsVariable<std::string>& default_alert_action() const;
   [[nodiscard]] SettingsVariable<std::string>& default_radar_site() const;
   [[nodiscard]] SettingsVariable<std::string>& default_time_zone() const;
   [[nodiscard]] SettingsContainer<std::vector<std::int64_t>>&
                                                 font_sizes() const;
   [[nodiscard]] SettingsVariable<std::int64_t>& grid_height() const;
   [[nodiscard]] SettingsVariable<std::int64_t>& grid_width() const;
   [[nodiscard]] SettingsVariable<std::int64_t>& loop_delay() const;
   [[nodiscard]] SettingsVariable<double>&       loop_speed() const;
   [[nodiscard]] SettingsVariable<std::int64_t>& loop_time() const;
   [[nodiscard]] SettingsVariable<std::string>&  map_provider() const;
   [[nodiscard]] SettingsVariable<std::string>&  mapbox_api_key() const;
   [[nodiscard]] SettingsVariable<std::string>&  maptiler_api_key() const;
   [[nodiscard]] SettingsVariable<std::int64_t>& nmea_baud_rate() const;
   [[nodiscard]] SettingsVariable<std::string>&  nmea_source() const;
   [[nodiscard]] SettingsVariable<std::string>&  positioning_plugin() const;
   [[nodiscard]] SettingsVariable<bool>&
   process_module_warnings_enabled() const;
   [[nodiscard]] SettingsVariable<std::string>& screen_capture_folder() const;
   [[nodiscard]] SettingsVariable<std::string>& screen_capture_name() const;
   [[nodiscard]] SettingsVariable<bool>& screen_capture_on_refresh() const;
   [[nodiscard]] SettingsVariable<bool>& show_map_attribution() const;
   [[nodiscard]] SettingsVariable<bool>& show_map_center() const;
   [[nodiscard]] SettingsVariable<bool>& show_map_logo() const;
   [[nodiscard]] SettingsVariable<std::string>& theme() const;
   [[nodiscard]] SettingsVariable<std::string>& theme_file() const;
   [[nodiscard]] SettingsVariable<bool>&        track_location() const;
   [[nodiscard]] SettingsVariable<bool>& update_notifications_enabled() const;
   [[nodiscard]] SettingsVariable<std::string>& warnings_provider() const;
   [[nodiscard]] SettingsVariable<bool>&        cursor_icon_always_on() const;
   [[nodiscard]] SettingsVariable<double>&      radar_site_threshold() const;
   [[nodiscard]] SettingsVariable<bool>& high_privilege_warning_enabled() const;
   [[nodiscard]] SettingsVariable<double>& cursor_icon_scale() const;

   static GeneralSettings& Instance();

   friend bool operator==(const GeneralSettings& lhs,
                          const GeneralSettings& rhs);

   bool Shutdown();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
