#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace scwx::qt::settings
{

class UiSettingsImpl;

class UiSettings : public SettingsCategory
{
public:
   explicit UiSettings();
   ~UiSettings() override;

   UiSettings(const UiSettings&)            = delete;
   UiSettings& operator=(const UiSettings&) = delete;

   UiSettings(UiSettings&&) noexcept;
   UiSettings& operator=(UiSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<bool>& level2_products_expanded() const;
   [[nodiscard]] SettingsVariable<bool>& level2_settings_expanded() const;
   [[nodiscard]] SettingsVariable<bool>& level3_products_expanded() const;
   [[nodiscard]] SettingsVariable<bool>& level3_settings_expanded() const;
   [[nodiscard]] SettingsVariable<bool>& map_settings_expanded() const;
   [[nodiscard]] SettingsVariable<bool>& timeline_expanded() const;
   [[nodiscard]] SettingsVariable<bool>&        spc_outlook_expanded() const;
   [[nodiscard]] SettingsVariable<std::string>& main_ui_state() const;
   [[nodiscard]] SettingsVariable<std::string>& main_ui_geometry() const;
   [[nodiscard]] SettingsVariable<std::int64_t>&
   radar_toolbox_dock_width() const;
   [[nodiscard]] SettingsVariable<std::string>& map_pane_splitter_state() const;
   [[nodiscard]] SettingsVariable<std::string>& map_pane_popout_state() const;
   [[nodiscard]] SettingsVariable<std::string>&
                                         map_pane_view_link_state() const;
   [[nodiscard]] SettingsVariable<bool>& panes_match_map_style() const;

   bool Shutdown();

   static UiSettings& Instance();

   friend bool operator==(const UiSettings& lhs, const UiSettings& rhs);

private:
   std::unique_ptr<UiSettingsImpl> p;
};

} // namespace scwx::qt::settings
