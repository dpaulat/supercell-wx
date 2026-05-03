#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <memory>

namespace scwx::qt::settings
{

class BlitzortungSettings : public SettingsCategory
{
public:
   explicit BlitzortungSettings();
   ~BlitzortungSettings() override;

   BlitzortungSettings(const BlitzortungSettings&)            = delete;
   BlitzortungSettings& operator=(const BlitzortungSettings&) = delete;

   BlitzortungSettings(BlitzortungSettings&&) noexcept;
   BlitzortungSettings& operator=(BlitzortungSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<bool>& enabled() const;

   static BlitzortungSettings& Instance();

   friend bool operator==(const BlitzortungSettings& lhs,
                          const BlitzortungSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
