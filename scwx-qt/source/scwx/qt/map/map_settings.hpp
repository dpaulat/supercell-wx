#pragma once

namespace scwx
{
namespace qt
{
namespace map
{

struct MapSettings
{
   explicit MapSettings() : isActive_ {false} {}
   ~MapSettings() = default;

   MapSettings(const MapSettings&) = delete;
   MapSettings& operator=(const MapSettings&) = delete;

   MapSettings(MapSettings&&) noexcept = default;
   MapSettings& operator=(MapSettings&&) noexcept = default;

   bool isActive_;
};

} // namespace map
} // namespace qt
} // namespace scwx
