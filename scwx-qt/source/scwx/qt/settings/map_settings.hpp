#pragma once

#include <scwx/common/products.hpp>
#include <scwx/qt/settings/settings_category.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

class MapSettingsImpl;

class MapSettings : public SettingsCategory
{
public:
   explicit MapSettings();
   ~MapSettings();

   MapSettings(const MapSettings&)            = delete;
   MapSettings& operator=(const MapSettings&) = delete;

   MapSettings(MapSettings&&) noexcept;
   MapSettings& operator=(MapSettings&&) noexcept;

   std::size_t               count() const;
   std::string               radar_site(std::size_t i) const;
   common::RadarProductGroup radar_product_group(std::size_t i) const;
   std::string               radar_product(std::size_t i) const;

   /**
    * Reads the variables from the JSON object.
    *
    * @param json JSON object to read
    *
    * @return true if the values read are valid, false if any values were
    * modified.
    */
   bool ReadJson(const boost::json::object& json) override;

   /**
    * Writes the variables to the JSON object.
    *
    * @param json JSON object to write
    */
   void WriteJson(boost::json::object& json) const override;

   friend bool operator==(const MapSettings& lhs, const MapSettings& rhs);

private:
   std::unique_ptr<MapSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
