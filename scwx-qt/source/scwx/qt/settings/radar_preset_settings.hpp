#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/types/map_types.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/json.hpp>

namespace scwx::qt::settings
{

class RadarPresetSettings : public SettingsCategory
{
public:
   static constexpr std::size_t kMapCount_ = types::kMapCount_;

   struct Preset
   {
      struct ProductEntry
      {
         std::string group;
         std::string name;
      };

      std::string                          name;
      std::int64_t                         gridWidth;
      std::int64_t                         gridHeight;
      std::vector<int>                     rowSizes;
      std::vector<int>                     columnSizes;
      std::array<ProductEntry, kMapCount_> products;
   };

   explicit RadarPresetSettings();
   ~RadarPresetSettings() override;

   RadarPresetSettings(const RadarPresetSettings&)            = delete;
   RadarPresetSettings& operator=(const RadarPresetSettings&) = delete;

   RadarPresetSettings(RadarPresetSettings&&) noexcept;
   RadarPresetSettings& operator=(RadarPresetSettings&&) noexcept;

   [[nodiscard]] std::vector<Preset>&       presets();
   [[nodiscard]] const std::vector<Preset>& presets() const;
   [[nodiscard]] std::string                active_preset() const;
   void set_active_preset(const std::string& name);

   [[nodiscard]] std::optional<std::size_t>
   FindPresetIndex(const std::string& name) const;

   bool ReadJson(const boost::json::object& json) override;
   void WriteJson(boost::json::object& json) const override;

   static RadarPresetSettings& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

void tag_invoke(boost::json::value_from_tag,
                boost::json::value&                              jv,
                const RadarPresetSettings::Preset::ProductEntry& product);
void tag_invoke(boost::json::value_from_tag,
                boost::json::value&                jv,
                const RadarPresetSettings::Preset& preset);

} // namespace scwx::qt::settings
