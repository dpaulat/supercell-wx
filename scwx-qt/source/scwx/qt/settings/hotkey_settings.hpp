#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/types/hotkey_types.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

class HotkeySettings : public SettingsCategory
{
public:
   explicit HotkeySettings();
   ~HotkeySettings();

   HotkeySettings(const HotkeySettings&)            = delete;
   HotkeySettings& operator=(const HotkeySettings&) = delete;

   HotkeySettings(HotkeySettings&&) noexcept;
   HotkeySettings& operator=(HotkeySettings&&) noexcept;

   SettingsVariable<std::string>& hotkey(scwx::qt::types::Hotkey hotkey) const;

   static HotkeySettings& Instance();

   friend bool operator==(const HotkeySettings& lhs, const HotkeySettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
