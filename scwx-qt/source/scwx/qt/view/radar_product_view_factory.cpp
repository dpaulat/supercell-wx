#include <scwx/qt/view/radar_product_view_factory.hpp>
#include <scwx/qt/view/level2_product_view.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ =
   "[scwx::qt::view::radar_product_view_factory] ";

typedef std::function<std::shared_ptr<RadarProductView>(
   const std::string&                            productName,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)>
   CreateRadarProductFunction;

std::shared_ptr<RadarProductView> RadarProductViewFactory::Create(
   common::RadarProductGroup                     productGroup,
   const std::string&                            productName,
   float                                         elevation,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   std::shared_ptr<RadarProductView> view = nullptr;

   if (productGroup == common::RadarProductGroup::Level2)
   {
      common::Level2Product product = common::GetLevel2Product(productName);

      if (product == common::Level2Product::Unknown)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Unknown Level 2 radar product: " << productName;
      }
      else
      {
         view = Create(product, elevation, radarProductManager);
      }
   }
   else
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Unknown radar product group: "
         << common::GetRadarProductGroupName(productGroup);
   }

   return view;
}

std::shared_ptr<RadarProductView> RadarProductViewFactory::Create(
   common::Level2Product                         product,
   float                                         elevation,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return Level2ProductView::Create(product, elevation, radarProductManager);
}

} // namespace view
} // namespace qt
} // namespace scwx
