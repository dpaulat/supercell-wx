#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <memory>
#include <string>

#include <boost/gil/typedefs.hpp>

namespace scwx::qt::settings
{

class ButtonSettings : public SettingsCategory
{
public:
   explicit ButtonSettings(const std::string& name);
   ~ButtonSettings() override;

   ButtonSettings(const ButtonSettings&)            = delete;
   ButtonSettings& operator=(const ButtonSettings&) = delete;

   ButtonSettings(ButtonSettings&&) noexcept;
   ButtonSettings& operator=(ButtonSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<std::string>& button_color() const;
   [[nodiscard]] SettingsVariable<std::string>& hover_color() const;
   [[nodiscard]] SettingsVariable<std::string>& active_color() const;

   [[nodiscard]] boost::gil::rgba8_pixel_t GetButtonColorRgba8() const;
   [[nodiscard]] boost::gil::rgba8_pixel_t GetHoverColorRgba8() const;
   [[nodiscard]] boost::gil::rgba8_pixel_t GetActiveColorRgba8() const;

   [[nodiscard]] boost::gil::rgba32f_pixel_t GetButtonColorRgba32f() const;
   [[nodiscard]] boost::gil::rgba32f_pixel_t GetHoverColorRgba32f() const;
   [[nodiscard]] boost::gil::rgba32f_pixel_t GetActiveColorRgba32f() const;

   void StageValues(const boost::gil::rgba8_pixel_t& buttonColor,
                    const boost::gil::rgba8_pixel_t& hoverColor,
                    const boost::gil::rgba8_pixel_t& activeColor);

   friend bool operator==(const ButtonSettings& lhs, const ButtonSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
