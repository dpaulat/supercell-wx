#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

class UiSettingsImpl;

class UiSettings : public SettingsCategory
{
public:
   explicit UiSettings();
   ~UiSettings();

   UiSettings(const UiSettings&)            = delete;
   UiSettings& operator=(const UiSettings&) = delete;

   UiSettings(UiSettings&&) noexcept;
   UiSettings& operator=(UiSettings&&) noexcept;

   SettingsVariable<bool>& level2_products_expanded() const;
   SettingsVariable<bool>& level2_settings_expanded() const;
   SettingsVariable<bool>& level3_products_expanded() const;
   SettingsVariable<bool>& map_settings_expanded() const;
   SettingsVariable<bool>& timeline_expanded() const;
   SettingsVariable<std::string>& main_ui_state() const;
   SettingsVariable<std::string>& main_ui_geometry() const;

   bool Shutdown();

   static UiSettings& Instance();

   friend bool operator==(const UiSettings& lhs, const UiSettings& rhs);

private:
   std::unique_ptr<UiSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
