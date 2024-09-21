#pragma once

#include <scwx/qt/settings/line_settings.hpp>
#include <scwx/qt/settings/settings_category.hpp>
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

class AlertPaletteSettings : public SettingsCategory
{
public:
   explicit AlertPaletteSettings(awips::Phenomenon phenomenon);
   ~AlertPaletteSettings();

   AlertPaletteSettings(const AlertPaletteSettings&)            = delete;
   AlertPaletteSettings& operator=(const AlertPaletteSettings&) = delete;

   AlertPaletteSettings(AlertPaletteSettings&&) noexcept;
   AlertPaletteSettings& operator=(AlertPaletteSettings&&) noexcept;

   LineSettings&
   threat_category(awips::ibw::ThreatCategory threatCategory) const;
   LineSettings& inactive() const;
   LineSettings& observed() const;
   LineSettings& tornado_possible() const;

   friend bool operator==(const AlertPaletteSettings& lhs,
                          const AlertPaletteSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
