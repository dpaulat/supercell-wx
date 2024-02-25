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

class ProductSettings : public SettingsCategory
{
public:
   explicit ProductSettings();
   ~ProductSettings();

   ProductSettings(const ProductSettings&)            = delete;
   ProductSettings& operator=(const ProductSettings&) = delete;

   ProductSettings(ProductSettings&&) noexcept;
   ProductSettings& operator=(ProductSettings&&) noexcept;

   SettingsVariable<bool>& sti_forecast_enabled() const;
   SettingsVariable<bool>& sti_past_enabled() const;

   static ProductSettings& Instance();

   friend bool operator==(const ProductSettings& lhs,
                          const ProductSettings& rhs);

   bool Shutdown();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
