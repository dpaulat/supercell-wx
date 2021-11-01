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

class RadarProductViewImpl
{
public:
   explicit RadarProductViewImpl() = default;
   ~RadarProductViewImpl()         = default;
};

RadarProductView::RadarProductView() :
    p(std::make_unique<RadarProductViewImpl>()) {};
RadarProductView::~RadarProductView() = default;

const std::vector<boost::gil::rgba8_pixel_t>&
RadarProductView::color_table() const
{
   return DEFAULT_COLOR_TABLE;
}

std::chrono::system_clock::time_point RadarProductView::sweep_time() const
{
   return {};
}

void RadarProductView::Initialize()
{
   ComputeSweep();
}

void RadarProductView::ComputeSweep()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "ComputeSweep()";

   emit SweepComputed();
}

} // namespace view
} // namespace qt
} // namespace scwx
