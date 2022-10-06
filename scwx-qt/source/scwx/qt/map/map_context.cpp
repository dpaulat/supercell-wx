#include <scwx/qt/map/map_context.hpp>

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
       settings_ {},
       radarProductView_ {radarProductView},
       radarProductGroup_ {common::RadarProductGroup::Unknown},
       radarProduct_ {"???"},
       radarProductCode_ {0}
   {
   }

   ~Impl() {}

   MapSettings                             settings_;
   std::shared_ptr<view::RadarProductView> radarProductView_;
   common::RadarProductGroup               radarProductGroup_;
   std::string                             radarProduct_;
   int16_t                                 radarProductCode_;
};

MapContext::MapContext(
   std::shared_ptr<view::RadarProductView> radarProductView) :
    p(std::make_unique<Impl>(radarProductView))
{
}
MapContext::~MapContext() = default;

MapContext::MapContext(MapContext&&) noexcept            = default;
MapContext& MapContext::operator=(MapContext&&) noexcept = default;

MapSettings& MapContext::settings()
{
   return p->settings_;
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

void MapContext::set_radar_product_view(
   std::shared_ptr<view::RadarProductView> radarProductView)
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

} // namespace map
} // namespace qt
} // namespace scwx
