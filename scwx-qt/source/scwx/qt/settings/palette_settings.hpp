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

class PaletteSettingsImpl;

class PaletteSettings : public SettingsCategory
{
public:
   explicit PaletteSettings();
   ~PaletteSettings();

   PaletteSettings(const PaletteSettings&)            = delete;
   PaletteSettings& operator=(const PaletteSettings&) = delete;

   PaletteSettings(PaletteSettings&&) noexcept;
   PaletteSettings& operator=(PaletteSettings&&) noexcept;

   SettingsVariable<std::string>& palette(const std::string& name) const;

   friend bool operator==(const PaletteSettings& lhs,
                          const PaletteSettings& rhs);

private:
   std::unique_ptr<PaletteSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
