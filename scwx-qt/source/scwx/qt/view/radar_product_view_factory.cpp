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

static const std::unordered_map<std::string, CreateRadarProductFunction>
   create_ {{PRODUCT_L2_REF, Level2ProductView::Create},
            {PRODUCT_L2_VEL, Level2ProductView::Create},
            {PRODUCT_L2_SW, Level2ProductView::Create},
            {PRODUCT_L2_ZDR, Level2ProductView::Create},
            {PRODUCT_L2_PHI, Level2ProductView::Create},
            {PRODUCT_L2_RHO, Level2ProductView::Create},
            {PRODUCT_L2_CFP, Level2ProductView::Create}};

std::shared_ptr<RadarProductView> RadarProductViewFactory::Create(
   const std::string&                            productName,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   std::shared_ptr<RadarProductView> view = nullptr;

   if (create_.find(productName) == create_.end())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Unknown radar product: " << productName;
   }
   else
   {
      view = create_.at(productName)(productName, radarProductManager);
   }

   return view;
}

} // namespace view
} // namespace qt
} // namespace scwx
