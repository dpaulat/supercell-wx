#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/awips/phenomenon.hpp>

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
   SettingsVariable<std::string>& alert_color(awips::Phenomenon phenomenon,
                                              bool              active) const;

   static const std::vector<awips::Phenomenon>& alert_phenomena();

   friend bool operator==(const PaletteSettings& lhs,
                          const PaletteSettings& rhs);

private:
   std::unique_ptr<PaletteSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
