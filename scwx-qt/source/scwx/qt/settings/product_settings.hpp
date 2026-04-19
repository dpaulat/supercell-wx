#pragma once

#include <scwx/common/products.hpp>
#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <memory>
#include <optional>
#include <string>

namespace scwx::qt::settings
{

class ProductSettings : public SettingsCategory
{
public:
   explicit ProductSettings();
   ~ProductSettings() override;

   ProductSettings(const ProductSettings&)            = delete;
   ProductSettings& operator=(const ProductSettings&) = delete;

   ProductSettings(ProductSettings&&) noexcept;
   ProductSettings& operator=(ProductSettings&&) noexcept;

   SettingsVariable<bool>& show_smoothed_range_folding();
   SettingsVariable<bool>& sti_forecast_enabled();
   SettingsVariable<bool>& sti_past_enabled();

   [[nodiscard]] std::optional<float>
        color_table_threshold(common::RadarProductGroup group,
                              const std::string&        productName) const;
   void set_color_table_threshold(common::RadarProductGroup group,
                                  const std::string&        productName,
                                  std::optional<float>      threshold);

   bool ReadJson(const boost::json::object& json) override;
   void WriteJson(boost::json::object& json) const override;

   static ProductSettings& Instance();

   friend bool operator==(const ProductSettings& lhs,
                          const ProductSettings& rhs);

   bool Shutdown();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
