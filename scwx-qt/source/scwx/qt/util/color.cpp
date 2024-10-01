#include <scwx/qt/util/color.hpp>

#include <fmt/format.h>
#include <re2/re2.h>
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
   return fmt::format(
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

boost::gil::rgba32f_pixel_t ToRgba32fPixelT(const std::string& argbString)
{
   boost::gil::rgba8_pixel_t rgba8Pixel = ToRgba8PixelT(argbString);
   return boost::gil::rgba32f_pixel_t {rgba8Pixel[0] / 255.0f,
                                       rgba8Pixel[1] / 255.0f,
                                       rgba8Pixel[2] / 255.0f,
                                       rgba8Pixel[3] / 255.0f};
}

bool ValidateArgbString(const std::string& argbString)
{
   static constexpr LazyRE2 re = {"#[0-9A-Fa-f]{8}"};
   return RE2::FullMatch(argbString, *re);
}

} // namespace color
} // namespace util
} // namespace qt
} // namespace scwx
