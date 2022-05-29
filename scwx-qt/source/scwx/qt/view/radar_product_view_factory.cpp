#include <scwx/qt/view/radar_product_view_factory.hpp>
#include <scwx/qt/view/level2_product_view.hpp>
#include <scwx/qt/view/level3_radial_view.hpp>
#include <scwx/qt/view/level3_raster_view.hpp>
#include <scwx/util/logger.hpp>

#include <unordered_set>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ =
   "scwx::qt::view::radar_product_view_factory";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

typedef std::function<std::shared_ptr<RadarProductView>(
   const std::string&                            productName,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)>
   CreateRadarProductFunction;

std::unordered_set<int16_t> level3GenericRadialProducts_ {176, 178, 179};
std::unordered_set<int16_t> level3RadialProducts_ {
   19,  20,  27,  30,  31,  32,  33,  34,  56,  78,  79,  80,  93,
   94,  99,  113, 132, 133, 134, 135, 137, 138, 144, 145, 146, 147,
   150, 151, 153, 154, 155, 159, 161, 163, 165, 167, 168, 169, 170,
   171, 172, 173, 174, 175, 177, 180, 181, 182, 186, 193, 195};
std::unordered_set<int16_t> level3RasterProducts_ {
   37, 38, 41, 49, 50, 51, 57, 65, 66, 67, 81, 86, 90, 97, 98};

std::shared_ptr<RadarProductView> RadarProductViewFactory::Create(
   common::RadarProductGroup                     productGroup,
   const std::string&                            productName,
   int16_t                                       productCode,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   std::shared_ptr<RadarProductView> view = nullptr;

   if (productGroup == common::RadarProductGroup::Level2)
   {
      common::Level2Product product = common::GetLevel2Product(productName);

      if (product == common::Level2Product::Unknown)
      {
         logger_->warn("Unknown Level 2 radar product: {}", productName);
      }
      else
      {
         view = Create(product, radarProductManager);
      }
   }
   else if (productGroup == common::RadarProductGroup::Level3)
   {
      if (level3RadialProducts_.contains(productCode))
      {
         view = Level3RadialView::Create(productName, radarProductManager);
      }
      else if (level3RasterProducts_.contains(productCode))
      {
         view = Level3RasterView::Create(productName, radarProductManager);
      }
   }
   else
   {
      logger_->warn("Unknown radar product group: {}",
                    common::GetRadarProductGroupName(productGroup));
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
