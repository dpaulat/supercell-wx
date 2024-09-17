#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/awips/phenomenon.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

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

   static PaletteSettings& Instance();

   friend bool operator==(const PaletteSettings& lhs,
                          const PaletteSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
