#include <scwx/qt/util/color.hpp>

#include <format>

namespace scwx
{
namespace qt
{
namespace util
{
namespace color
{

static const std::string logPrefix_ = "scwx::qt::util::color";

std::string ToArgbString(const boost::gil::rgba8_pixel_t& color)
{
   return std::format(
      "#{:02x}{:02x}{:02x}{:02x}", color[3], color[0], color[1], color[2]);
}

} // namespace color
} // namespace util
} // namespace qt
} // namespace scwx
