#include <scwx/qt/map/map_context.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class MapContext::Impl
{
public:
   explicit Impl(std::shared_ptr<view::RadarProductView> radarProductView) :
       radarProductView_ {radarProductView}
   {
   }

   ~Impl() {}

   std::weak_ptr<QMapLibreGL::Map> map_ {};
   MapSettings                     settings_ {};
   float                           pixelRatio_ {1.0f};
   common::RadarProductGroup       radarProductGroup_ {
      common::RadarProductGroup::Unknown};
   std::string                              radarProduct_ {"???"};
   int16_t                                  radarProductCode_ {0};
   QMapLibreGL::CustomLayerRenderParameters renderParameters_ {};

   std::shared_ptr<view::OverlayProductView> overlayProductView_ {nullptr};
   std::shared_ptr<view::RadarProductView>   radarProductView_;
};

MapContext::MapContext(
   std::shared_ptr<view::RadarProductView> radarProductView) :
    p(std::make_unique<Impl>(radarProductView))
{
}
MapContext::~MapContext() = default;

MapContext::MapContext(MapContext&&) noexcept            = default;
MapContext& MapContext::operator=(MapContext&&) noexcept = default;

std::weak_ptr<QMapLibreGL::Map> MapContext::map() const
{
   return p->map_;
}

MapSettings& MapContext::settings()
{
   return p->settings_;
}

float MapContext::pixel_ratio() const
{
   return p->pixelRatio_;
}

std::shared_ptr<view::OverlayProductView>
MapContext::overlay_product_view() const
{
   return p->overlayProductView_;
}

std::shared_ptr<view::RadarProductView> MapContext::radar_product_view() const
{
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

int16_t MapContext::radar_product_code() const
{
   return p->radarProductCode_;
}

QMapLibreGL::CustomLayerRenderParameters MapContext::render_parameters() const
{
   return p->renderParameters_;
}

void MapContext::set_map(const std::shared_ptr<QMapLibreGL::Map>& map)
{
   p->map_ = map;
}

void MapContext::set_overlay_product_view(
   const std::shared_ptr<view::OverlayProductView>& overlayProductView)
{
   p->overlayProductView_ = overlayProductView;
}

void MapContext::set_pixel_ratio(float pixelRatio)
{
   p->pixelRatio_ = pixelRatio;
}

void MapContext::set_radar_product_view(
   const std::shared_ptr<view::RadarProductView>& radarProductView)
{
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

void MapContext::set_render_parameters(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   p->renderParameters_ = params;
}

} // namespace map
} // namespace qt
} // namespace scwx
