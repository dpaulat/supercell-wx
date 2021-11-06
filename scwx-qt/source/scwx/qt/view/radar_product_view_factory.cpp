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
   const std::string&                            productGroup,
   const std::string&                            productName,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   std::shared_ptr<RadarProductView> view = nullptr;

   if (productGroup == "L2")
   {
      common::Level2Product product = common::GetLevel2Product(productName);

      if (product == common::Level2Product::Unknown)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Unknown Level 2 radar product: " << productName;
      }
      else
      {
         view = Create(product, radarProductManager);
      }
   }
   else
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Unknown radar product group: " << productGroup;
   }

   return view;
}

std::shared_ptr<RadarProductView> RadarProductViewFactory::Create(
   common::Level2Product                         product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return Level2ProductView::Create(product, radarProductManager);
}

} // namespace view
} // namespace qt
} // namespace scwx
