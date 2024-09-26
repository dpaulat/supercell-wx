#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <memory>
#include <string>

#include <boost/gil/typedefs.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

class LineSettings : public SettingsCategory
{
public:
   explicit LineSettings(const std::string& name);
   ~LineSettings();

   LineSettings(const LineSettings&)            = delete;
   LineSettings& operator=(const LineSettings&) = delete;

   LineSettings(LineSettings&&) noexcept;
   LineSettings& operator=(LineSettings&&) noexcept;

   SettingsVariable<std::string>& border_color() const;
   SettingsVariable<std::string>& highlight_color() const;
   SettingsVariable<std::string>& line_color() const;

   SettingsVariable<std::int64_t>& border_width() const;
   SettingsVariable<std::int64_t>& highlight_width() const;
   SettingsVariable<std::int64_t>& line_width() const;

   void StageValues(boost::gil::rgba8_pixel_t borderColor,
                    boost::gil::rgba8_pixel_t highlightColor,
                    boost::gil::rgba8_pixel_t lineColor,
                    std::int64_t              borderWidth,
                    std::int64_t              highlightWidth,
                    std::int64_t              lineWidth);

   friend bool operator==(const LineSettings& lhs, const LineSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
