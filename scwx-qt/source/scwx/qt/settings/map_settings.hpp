#pragma once

#include <memory>
#include <string>

#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

class MapSettingsImpl;

class MapSettings
{
public:
   explicit MapSettings();
   ~MapSettings();

   MapSettings(const MapSettings&) = delete;
   MapSettings& operator=(const MapSettings&) = delete;

   MapSettings(MapSettings&&) noexcept;
   MapSettings& operator=(MapSettings&&) noexcept;

   size_t      count() const;
   std::string radar_site(size_t i) const;
   std::string radar_product_group(size_t i) const;
   std::string radar_product(size_t i) const;

   boost::json::value ToJson() const;

   static std::shared_ptr<MapSettings> Create();
   static std::shared_ptr<MapSettings> Load(const boost::json::value* json,
                                            bool& jsonDirty);

   friend bool operator==(const MapSettings& lhs, const MapSettings& rhs);

private:
   std::unique_ptr<MapSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
