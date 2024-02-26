#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/logger.hpp>

#include <boost/asio.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ = "scwx::qt::view::radar_product_view";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Default color table should be transparent to prevent flicker
static const std::vector<boost::gil::rgba8_pixel_t> kDefaultColorTable_ = {
   boost::gil::rgba8_pixel_t(0, 128, 0, 0),
   boost::gil::rgba8_pixel_t(255, 192, 0, 0),
   boost::gil::rgba8_pixel_t(255, 0, 0, 0)};
static const std::uint16_t kDefaultColorTableMin_ = 2u;
static const std::uint16_t kDefaultColorTableMax_ = 255u;

class RadarProductViewImpl
{
public:
   explicit RadarProductViewImpl(
      std::shared_ptr<manager::RadarProductManager> radarProductManager) :
       initialized_ {false},
       sweepMutex_ {},
       selectedTime_ {},
       radarProductManager_ {radarProductManager}
   {
   }
   ~RadarProductViewImpl() {}

   bool       initialized_;
   std::mutex sweepMutex_;

   std::chrono::system_clock::time_point selectedTime_;

   std::shared_ptr<manager::RadarProductManager> radarProductManager_;
};

RadarProductView::RadarProductView(
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    p(std::make_unique<RadarProductViewImpl>(radarProductManager)) {};
RadarProductView::~RadarProductView() = default;

const std::vector<boost::gil::rgba8_pixel_t>&
RadarProductView::color_table_lut() const
{
   return kDefaultColorTable_;
}

std::uint16_t RadarProductView::color_table_min() const
{
   return kDefaultColorTableMin_;
}

std::uint16_t RadarProductView::color_table_max() const
{
   return kDefaultColorTableMax_;
}

float RadarProductView::elevation() const
{
   return 0.0f;
}

std::shared_ptr<manager::RadarProductManager>
RadarProductView::radar_product_manager() const
{
   return p->radarProductManager_;
}

float RadarProductView::range() const
{
   return 0.0f;
}

std::chrono::system_clock::time_point RadarProductView::selected_time() const
{
   return p->selectedTime_;
}

std::chrono::system_clock::time_point RadarProductView::sweep_time() const
{
   return {};
}

std::mutex& RadarProductView::sweep_mutex()
{
   return p->sweepMutex_;
}

void RadarProductView::set_radar_product_manager(
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   DisconnectRadarProductManager();
   p->radarProductManager_ = radarProductManager;
   ConnectRadarProductManager();
}

void RadarProductView::Initialize()
{
   ComputeSweep();

   p->initialized_ = true;
}

void RadarProductView::SelectElevation(float /*elevation*/) {}

void RadarProductView::SelectTime(std::chrono::system_clock::time_point time)
{
   p->selectedTime_ = time;
}

void RadarProductView::Update()
{
   boost::asio::post(thread_pool(), [this]() { ComputeSweep(); });
}

bool RadarProductView::IsInitialized() const
{
   return p->initialized_;
}

std::vector<float> RadarProductView::GetElevationCuts() const
{
   return {};
}

std::tuple<const void*, std::size_t, std::size_t>
RadarProductView::GetCfpMomentData() const
{
   const void* data          = nullptr;
   std::size_t dataSize      = 0;
   std::size_t componentSize = 0;

   return std::tie(data, dataSize, componentSize);
}

bool RadarProductView::IgnoreUnits() const
{
   return false;
}

std::vector<std::pair<std::string, std::string>>
RadarProductView::GetDescriptionFields() const
{
   return {};
}

void RadarProductView::ComputeSweep()
{
   logger_->debug("ComputeSweep()");

   Q_EMIT SweepComputed();
}

} // namespace view
} // namespace qt
} // namespace scwx
