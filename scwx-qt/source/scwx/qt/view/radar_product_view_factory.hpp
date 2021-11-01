#pragma once

#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/view/radar_product_view.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace view
{

class RadarProductViewFactory
{
private:
   explicit RadarProductViewFactory() = delete;
   ~RadarProductViewFactory()         = delete;

   RadarProductViewFactory(const RadarProductViewFactory&) = delete;
   RadarProductViewFactory& operator=(const RadarProductViewFactory&) = delete;

   RadarProductViewFactory(RadarProductViewFactory&&) noexcept = delete;
   RadarProductViewFactory&
   operator=(RadarProductViewFactory&&) noexcept = delete;

public:
   static std::shared_ptr<RadarProductView>
   Create(const std::string&                            productName,
          std::shared_ptr<manager::RadarProductManager> radarProductManager);
};

} // namespace view
} // namespace qt
} // namespace scwx
