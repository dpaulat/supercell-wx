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

class TextSettings : public SettingsCategory
{
public:
   explicit TextSettings();
   ~TextSettings();

   TextSettings(const TextSettings&)            = delete;
   TextSettings& operator=(const TextSettings&) = delete;

   TextSettings(TextSettings&&) noexcept;
   TextSettings& operator=(TextSettings&&) noexcept;

   SettingsVariable<std::int64_t>& hover_text_wrap() const;

   static TextSettings& Instance();

   friend bool operator==(const TextSettings& lhs, const TextSettings& rhs);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
