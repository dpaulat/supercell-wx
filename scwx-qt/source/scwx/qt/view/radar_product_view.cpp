#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/qt/settings/product_settings.hpp>
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
      RadarProductView*                             self,
      std::shared_ptr<manager::RadarProductManager> radarProductManager) :
       self_ {self},
       initialized_ {false},
       sweepMutex_ {},
       selectedTime_ {},
       radarProductManager_ {radarProductManager}
   {
      auto& productSettings = settings::ProductSettings::Instance();
      connection_           = productSettings.changed_signal().connect(
         [this]()
         {
            showSmoothedRangeFolding_ = settings::ProductSettings::Instance()
                                           .show_smoothed_range_folding()
                                           .GetValue();
            self_->Update();
         });
      ;
   }
   ~RadarProductViewImpl() {}

   void DisconnectProductSettings() { connection_.disconnect(); }

   RadarProductView* self_;

   bool       initialized_;
   std::mutex sweepMutex_;

   std::chrono::system_clock::time_point selectedTime_;
   bool                                  showSmoothedRangeFolding_ {false};
   bool                                  smoothingEnabled_ {false};
   types::RadarProductLoadStatus         loadStatus_ {
      types::RadarProductLoadStatus::ProductNotLoaded};

   std::optional<float> colorTableThreshold_ {};

   std::shared_ptr<manager::RadarProductManager> radarProductManager_;

   boost::signals2::scoped_connection connection_;
};

RadarProductView::RadarProductView(
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    p(std::make_unique<RadarProductViewImpl>(this, radarProductManager)) {};
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

std::optional<float> RadarProductView::elevation() const
{
   return {};
}

types::RadarProductLoadStatus RadarProductView::load_status() const
{
   return p->loadStatus_;
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

bool RadarProductView::show_smoothed_range_folding() const
{
   return p->showSmoothedRangeFolding_;
}

bool RadarProductView::smoothing_enabled() const
{
   return p->smoothingEnabled_;
}

std::chrono::system_clock::time_point RadarProductView::sweep_time() const
{
   return {};
}

std::mutex& RadarProductView::sweep_mutex()
{
   return p->sweepMutex_;
}

void RadarProductView::set_load_status(types::RadarProductLoadStatus loadStatus)
{
   p->loadStatus_ = loadStatus;
}

void RadarProductView::DisconnectProductSettings()
{
   p->DisconnectProductSettings();
}

void RadarProductView::set_radar_product_manager(
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   DisconnectRadarProductManager();
   p->radarProductManager_ = radarProductManager;
   ConnectRadarProductManager();
}

void RadarProductView::set_smoothing_enabled(bool smoothingEnabled)
{
   p->smoothingEnabled_ = smoothingEnabled;
}

std::optional<float> RadarProductView::color_table_threshold() const
{
   return p->colorTableThreshold_;
}

void RadarProductView::set_color_table_threshold(std::optional<float> threshold)
{
   p->colorTableThreshold_ = threshold;
   UpdateColorTableLut();
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
   boost::asio::post(thread_pool(),
                     [this]()
                     {
                        try
                        {
                           ComputeSweep();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
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

std::pair<float, float> RadarProductView::GetColorTableRange() const
{
   return {-std::numeric_limits<float>::infinity(),
           std::numeric_limits<float>::infinity()};
}

void RadarProductView::ComputeSweep()
{
   logger_->debug("ComputeSweep()");

   Q_EMIT SweepComputed();
}

} // namespace view
} // namespace qt
} // namespace scwx
