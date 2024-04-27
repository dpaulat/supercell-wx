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

class UnitSettings : public SettingsCategory
{
public:
   explicit UnitSettings();
   ~UnitSettings();

   UnitSettings(const UnitSettings&)            = delete;
   UnitSettings& operator=(const UnitSettings&) = delete;

   UnitSettings(UnitSettings&&) noexcept;
   UnitSettings& operator=(UnitSettings&&) noexcept;

   SettingsVariable<std::string>& accumulation_units() const;
   SettingsVariable<std::string>& echo_tops_units() const;
   SettingsVariable<std::string>& speed_units() const;

   static UnitSettings& Instance();

   friend bool operator==(const UnitSettings& lhs, const UnitSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
