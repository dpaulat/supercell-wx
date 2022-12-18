#pragma once

#include <scwx/qt/settings/settings_category.hpp>

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

   bool                 debug_enabled() const;
   std::string          default_radar_site() const;
   std::vector<int64_t> font_sizes() const;
   std::int64_t         grid_height() const;
   std::int64_t         grid_width() const;
   std::string          mapbox_api_key() const;

   friend bool operator==(const GeneralSettings& lhs,
                          const GeneralSettings& rhs);

private:
   std::unique_ptr<GeneralSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
