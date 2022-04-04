#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/common/constants.hpp>

#include <boost/log/trivial.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ = "[scwx::qt::view::radar_product_view] ";

static const std::vector<boost::gil::rgba8_pixel_t> DEFAULT_COLOR_TABLE = {
   boost::gil::rgba8_pixel_t(0, 128, 0, 255),
   boost::gil::rgba8_pixel_t(255, 192, 0, 255),
   boost::gil::rgba8_pixel_t(255, 0, 0, 255)};
static const uint16_t DEFAULT_COLOR_TABLE_MIN = 2u;
static const uint16_t DEFAULT_COLOR_TABLE_MAX = 255u;

class RadarProductViewImpl
{
public:
   explicit RadarProductViewImpl() : initialized_ {false}, sweepMutex_ {} {}
   ~RadarProductViewImpl() = default;

   bool       initialized_;
   std::mutex sweepMutex_;
};

RadarProductView::RadarProductView() :
    p(std::make_unique<RadarProductViewImpl>()) {};
RadarProductView::~RadarProductView() = default;

const std::vector<boost::gil::rgba8_pixel_t>&
RadarProductView::color_table() const
{
   return DEFAULT_COLOR_TABLE;
}

uint16_t RadarProductView::color_table_min() const
{
   return DEFAULT_COLOR_TABLE_MIN;
}

uint16_t RadarProductView::color_table_max() const
{
   return DEFAULT_COLOR_TABLE_MAX;
}

float RadarProductView::elevation() const
{
   return 0.0f;
}

float RadarProductView::range() const
{
   return 0.0f;
}

std::chrono::system_clock::time_point RadarProductView::sweep_time() const
{
   return {};
}

std::mutex& RadarProductView::sweep_mutex()
{
   return p->sweepMutex_;
}

void RadarProductView::Initialize()
{
   ComputeSweep();

   p->initialized_ = true;
}

void RadarProductView::SelectElevation(float elevation) {}

bool RadarProductView::IsInitialized() const
{
   return p->initialized_;
}

std::vector<float> RadarProductView::GetElevationCuts() const
{
   return {};
}

std::tuple<const void*, size_t, size_t>
RadarProductView::GetCfpMomentData() const
{
   const void* data          = nullptr;
   size_t      dataSize      = 0;
   size_t      componentSize = 0;

   return std::tie(data, dataSize, componentSize);
}

void RadarProductView::ComputeSweep()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "ComputeSweep()";

   emit SweepComputed();
}

} // namespace view
} // namespace qt
} // namespace scwx
