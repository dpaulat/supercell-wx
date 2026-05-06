#include <scwx/qt/map/map_context.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

#include <mutex>

namespace scwx::qt::map
{

class MapContext::Impl
{
public:
   explicit Impl(std::shared_ptr<view::RadarProductView> radarProductView) :
       radarProductView_ {radarProductView}
   {
   }

   ~Impl() {}

   std::weak_ptr<QMapLibre::Map> map_ {};
   MapSettings                   settings_ {};
   float                         pixelRatio_ {1.0f};
   common::RadarProductGroup     radarProductGroup_ {
      common::RadarProductGroup::Unknown};
   std::string                        radarProduct_ {"???"};
   int16_t                            radarProductCode_ {0};
   std::shared_ptr<config::RadarSite> radarSite_ {nullptr};
   bool                               screenCapture_ {false};

   MapProvider mapProvider_ {MapProvider::Unknown};
   std::string mapCopyrights_ {};

   QMargins           colorTableMargins_ {};
   common::Coordinate mouseCoordinate_ {};

   mutable std::mutex productViewMutex_ {};

   std::shared_ptr<view::OverlayProductView> overlayProductView_ {nullptr};
   std::shared_ptr<view::RadarProductView>   radarProductView_;

   QWidget* widget_;
};

MapContext::MapContext(
   std::shared_ptr<view::RadarProductView> radarProductView) :
    p(std::make_unique<Impl>(radarProductView))
{
}
MapContext::~MapContext() = default;

MapContext::MapContext(MapContext&&) noexcept            = default;
MapContext& MapContext::operator=(MapContext&&) noexcept = default;

std::weak_ptr<QMapLibre::Map> MapContext::map() const
{
   return p->map_;
}

std::string MapContext::map_copyrights() const
{
   return p->mapCopyrights_;
}

MapProvider MapContext::map_provider() const
{
   return p->mapProvider_;
}

MapSettings& MapContext::settings()
{
   return p->settings_;
}

QMargins MapContext::color_table_margins() const
{
   return p->colorTableMargins_;
}

float MapContext::pixel_ratio() const
{
   return p->pixelRatio_;
}

common::Coordinate MapContext::mouse_coordinate() const
{
   return p->mouseCoordinate_;
}

std::shared_ptr<view::OverlayProductView>
MapContext::overlay_product_view() const
{
   const std::scoped_lock lock {p->productViewMutex_};
   return p->overlayProductView_;
}

std::shared_ptr<view::RadarProductView> MapContext::radar_product_view() const
{
   const std::scoped_lock lock {p->productViewMutex_};
   return p->radarProductView_;
}

common::RadarProductGroup MapContext::radar_product_group() const
{
   return p->radarProductGroup_;
}

std::string MapContext::radar_product() const
{
   return p->radarProduct_;
}

std::shared_ptr<config::RadarSite> MapContext::radar_site() const
{
   return p->radarSite_;
}

int16_t MapContext::radar_product_code() const
{
   return p->radarProductCode_;
}

bool MapContext::screen_capture() const
{
   return p->screenCapture_;
}

QWidget* MapContext::widget() const
{
   return p->widget_;
}

void MapContext::set_map(const std::shared_ptr<QMapLibre::Map>& map)
{
   p->map_ = map;
}

void MapContext::set_map_copyrights(const std::string& copyrights)
{
   p->mapCopyrights_ = copyrights;
}

void MapContext::set_map_provider(MapProvider provider)
{
   p->mapProvider_ = provider;
}

void MapContext::set_color_table_margins(const QMargins& margins)
{
   p->colorTableMargins_ = margins;
}

void MapContext::set_mouse_coordinate(const common::Coordinate& coordinate)
{
   p->mouseCoordinate_ = coordinate;
}

void MapContext::set_overlay_product_view(
   const std::shared_ptr<view::OverlayProductView>& overlayProductView)
{
   const std::scoped_lock lock {p->productViewMutex_};
   p->overlayProductView_ = overlayProductView;
}

void MapContext::set_pixel_ratio(float pixelRatio)
{
   p->pixelRatio_ = pixelRatio;
}

void MapContext::set_radar_product_view(
   const std::shared_ptr<view::RadarProductView>& radarProductView)
{
   const std::scoped_lock lock {p->productViewMutex_};
   p->radarProductView_ = radarProductView;
}

void MapContext::set_radar_product_group(
   common::RadarProductGroup radarProductGroup)
{
   p->radarProductGroup_ = radarProductGroup;
}

void MapContext::set_radar_product(const std::string& radarProduct)
{
   p->radarProduct_ = radarProduct;
}

void MapContext::set_radar_product_code(int16_t radarProductCode)
{
   p->radarProductCode_ = radarProductCode;
}

void MapContext::set_radar_site(const std::shared_ptr<config::RadarSite>& site)
{
   p->radarSite_ = site;
}

void MapContext::set_screen_capture(bool screenCapture)
{
   p->screenCapture_ = screenCapture;
}

void MapContext::set_widget(QWidget* widget)
{
   p->widget_ = widget;
}

} // namespace scwx::qt::map
