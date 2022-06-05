#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

struct MapContext
{
   explicit MapContext(
      std::shared_ptr<view::RadarProductView> radarProductView = nullptr) :
       gl_ {},
       settings_ {},
       radarProductView_ {radarProductView},
       radarProductGroup_ {common::RadarProductGroup::Unknown},
       radarProduct_ {"???"},
       radarProductCode_ {0}
   {
   }
   ~MapContext() = default;

   MapContext(const MapContext&) = delete;
   MapContext& operator=(const MapContext&) = delete;

   MapContext(MapContext&&) noexcept = default;
   MapContext& operator=(MapContext&&) noexcept = default;

   gl::OpenGLFunctions                     gl_;
   MapSettings                             settings_;
   std::shared_ptr<view::RadarProductView> radarProductView_;
   common::RadarProductGroup               radarProductGroup_;
   std::string                             radarProduct_;
   int16_t                                 radarProductCode_;
};

} // namespace map
} // namespace qt
} // namespace scwx
