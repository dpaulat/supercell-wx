#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

#include <QMapLibreGL/QMapLibreGL>

namespace scwx
{
namespace qt
{
namespace map
{

class MapContext : public gl::GlContext
{
public:
   explicit MapContext(
      std::shared_ptr<view::RadarProductView> radarProductView = nullptr);
   ~MapContext();

   MapContext(const MapContext&)            = delete;
   MapContext& operator=(const MapContext&) = delete;

   MapContext(MapContext&&) noexcept;
   MapContext& operator=(MapContext&&) noexcept;

   std::weak_ptr<QMapLibreGL::Map>         map() const;
   MapSettings&                            settings();
   std::shared_ptr<view::RadarProductView> radar_product_view() const;
   common::RadarProductGroup               radar_product_group() const;
   std::string                             radar_product() const;
   int16_t                                 radar_product_code() const;

   void set_map(std::shared_ptr<QMapLibreGL::Map> map);
   void set_radar_product_view(
      std::shared_ptr<view::RadarProductView> radarProductView);
   void set_radar_product_group(common::RadarProductGroup radarProductGroup);
   void set_radar_product(const std::string& radarProduct);
   void set_radar_product_code(int16_t radarProductCode);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
