#pragma once

#include <scwx/qt/map/map_provider.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/common/products.hpp>
#include <scwx/qt/config/radar_site.hpp>

#include <qmaplibre.hpp>
#include <QMargins>

namespace scwx::qt::view
{

class OverlayProductView;
class RadarProductView;

} // namespace scwx::qt::view

namespace scwx::qt::map
{

struct MapSettings;

class MapContext
{
public:
   explicit MapContext(
      std::shared_ptr<view::RadarProductView> radarProductView = nullptr);
   ~MapContext();

   MapContext(const MapContext&)            = delete;
   MapContext& operator=(const MapContext&) = delete;

   MapContext(MapContext&&) noexcept;
   MapContext& operator=(MapContext&&) noexcept;

   [[nodiscard]] std::weak_ptr<QMapLibre::Map> map() const;
   [[nodiscard]] std::string                   map_copyrights() const;
   [[nodiscard]] MapProvider                   map_provider() const;
   [[nodiscard]] MapSettings&                  settings();
   [[nodiscard]] QMargins                      color_table_margins() const;
   [[nodiscard]] float                         pixel_ratio() const;
   [[nodiscard]] common::Coordinate            mouse_coordinate() const;
   [[nodiscard]] std::shared_ptr<view::OverlayProductView>
   overlay_product_view() const;
   [[nodiscard]] std::shared_ptr<view::RadarProductView>
                                                    radar_product_view() const;
   [[nodiscard]] common::RadarProductGroup          radar_product_group() const;
   [[nodiscard]] std::string                        radar_product() const;
   [[nodiscard]] int16_t                            radar_product_code() const;
   [[nodiscard]] std::shared_ptr<config::RadarSite> radar_site() const;
   [[nodiscard]] bool                               screen_capture() const;
   [[nodiscard]] QWidget*                           widget() const;

   void set_map(const std::shared_ptr<QMapLibre::Map>& map);
   void set_map_copyrights(const std::string& copyrights);
   void set_map_provider(MapProvider provider);
   void set_color_table_margins(const QMargins& margins);
   void set_mouse_coordinate(const common::Coordinate& coordinate);
   void set_overlay_product_view(
      const std::shared_ptr<view::OverlayProductView>& overlayProductView);
   void set_pixel_ratio(float pixelRatio);
   void set_radar_product_view(
      const std::shared_ptr<view::RadarProductView>& radarProductView);
   void set_radar_product_group(common::RadarProductGroup radarProductGroup);
   void set_radar_product(const std::string& radarProduct);
   void set_radar_product_code(int16_t radarProductCode);
   void set_radar_site(const std::shared_ptr<config::RadarSite>& site);
   void set_screen_capture(bool screenCapture);
   void set_widget(QWidget* widget);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
