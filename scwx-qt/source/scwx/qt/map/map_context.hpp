#pragma once

#include <scwx/qt/gl/gl.hpp>
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
       gl_ {}, radarProductView_ {radarProductView}
   {
   }
   ~MapContext() = default;

   MapContext(const MapContext&) = delete;
   MapContext& operator=(const MapContext&) = delete;

   MapContext(MapContext&&) noexcept = default;
   MapContext& operator=(MapContext&&) noexcept = default;

   gl::OpenGLFunctions                     gl_;
   std::shared_ptr<view::RadarProductView> radarProductView_;
};

} // namespace map
} // namespace qt
} // namespace scwx
