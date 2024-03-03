#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/common/products.hpp>

#include <qmaplibre.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

class OverlayProductView;
class RadarProductView;

} // namespace view

namespace map
{

struct MapSettings;

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

   std::weak_ptr<QMapLibre::Map>             map() const;
   std::string                               map_copyrights() const;
   MapProvider                               map_provider() const;
   MapSettings&                              settings();
   float                                     pixel_ratio() const;
   std::shared_ptr<view::OverlayProductView> overlay_product_view() const;
   std::shared_ptr<view::RadarProductView>   radar_product_view() const;
   common::RadarProductGroup                 radar_product_group() const;
   std::string                               radar_product() const;
   int16_t                                   radar_product_code() const;
   QMapLibre::CustomLayerRenderParameters    render_parameters() const;

   void set_map(const std::shared_ptr<QMapLibre::Map>& map);
   void set_map_copyrights(const std::string& copyrights);
   void set_map_provider(MapProvider provider);
   void set_overlay_product_view(
      const std::shared_ptr<view::OverlayProductView>& overlayProductView);
   void set_pixel_ratio(float pixelRatio);
   void set_radar_product_view(
      const std::shared_ptr<view::RadarProductView>& radarProductView);
   void set_radar_product_group(common::RadarProductGroup radarProductGroup);
   void set_radar_product(const std::string& radarProduct);
   void set_radar_product_code(int16_t radarProductCode);
   void
   set_render_parameters(const QMapLibre::CustomLayerRenderParameters& params);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
