#include <scwx/qt/util/color.hpp>

#include <format>
#include <QColor>

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

boost::gil::rgba8_pixel_t ToRgba8PixelT(const std::string& argbString)
{
   QRgb color = QColor(QString::fromStdString(argbString)).rgba();
   return boost::gil::rgba8_pixel_t {static_cast<uint8_t>(qRed(color)),
                                     static_cast<uint8_t>(qGreen(color)),
                                     static_cast<uint8_t>(qBlue(color)),
                                     static_cast<uint8_t>(qAlpha(color))};
}

} // namespace color
} // namespace util
} // namespace qt
} // namespace scwx
