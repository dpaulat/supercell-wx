#include <scwx/qt/manager/radar_product_manager_notifier.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ =
   "scwx::qt::manager::radar_product_manager_notifier";

class RadarProductManagerNotifierImpl
{
public:
   explicit RadarProductManagerNotifierImpl() {}
   ~RadarProductManagerNotifierImpl() {}
};

RadarProductManagerNotifier::RadarProductManagerNotifier() :
    p(std::make_unique<RadarProductManagerNotifierImpl>())
{
}
RadarProductManagerNotifier::~RadarProductManagerNotifier() = default;

RadarProductManagerNotifier& RadarProductManagerNotifier::Instance()
{
   static RadarProductManagerNotifier instance_ {};
   return instance_;
}

} // namespace manager
} // namespace qt
} // namespace scwx
