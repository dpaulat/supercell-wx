#include <scwx/qt/util/color.hpp>

#include <fmt/format.h>
#include <re2/re2.h>
#include <QColor>

namespace scwx::qt::util::color
{

boost::gil::rgba8_pixel_t Blend(const boost::gil::rgba8_pixel_t& foreground,
                                const boost::gil::rgba8_pixel_t& background)
{
   boost::gil::rgba8_pixel_t color {};

   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   const float fgAlpha = static_cast<float>(foreground[3]) / 255.0f;
   const float bgAlpha = 1.0f - fgAlpha;

   color[0] =
      static_cast<std::uint8_t>(static_cast<float>(foreground[0]) * fgAlpha +
                                static_cast<float>(background[0]) * bgAlpha);
   color[1] =
      static_cast<std::uint8_t>(static_cast<float>(foreground[1]) * fgAlpha +
                                static_cast<float>(background[1]) * bgAlpha);
   color[2] =
      static_cast<std::uint8_t>(static_cast<float>(foreground[2]) * fgAlpha +
                                static_cast<float>(background[2]) * bgAlpha);
   color[3] = 255;
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

   return color;
}

std::string ToArgbString(const boost::gil::rgba8_pixel_t& color)
{
   return fmt::format(
      "#{:02x}{:02x}{:02x}{:02x}", color[3], color[0], color[1], color[2]);
}

ImVec4 ToImVec4(const boost::gil::rgba8_pixel_t& color)
{
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   return ImVec4 {static_cast<float>(color[0]) / 255.0f,
                  static_cast<float>(color[1]) / 255.0f,
                  static_cast<float>(color[2]) / 255.0f,
                  static_cast<float>(color[3]) / 255.0f};
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

ImVec4 ToImVec4(const std::string& argbString)
{
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   const QRgb color = QColor(QString::fromStdString(argbString)).rgba();
   return ImVec4 {static_cast<float>(qRed(color)) / 255.0f,
                  static_cast<float>(qGreen(color)) / 255.0f,
                  static_cast<float>(qBlue(color)) / 255.0f,
                  static_cast<float>(qAlpha(color)) / 255.0f};
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
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
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   boost::gil::rgba8_pixel_t rgba8Pixel = ToRgba8PixelT(argbString);
   return boost::gil::rgba32f_pixel_t {
      static_cast<float>(rgba8Pixel[0]) / 255.0f,
      static_cast<float>(rgba8Pixel[1]) / 255.0f,
      static_cast<float>(rgba8Pixel[2]) / 255.0f,
      static_cast<float>(rgba8Pixel[3]) / 255.0f};
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

bool ValidateArgbString(const std::string& argbString)
{
   static constexpr LazyRE2 re = {"#[0-9A-Fa-f]{8}"};
   return RE2::FullMatch(argbString, *re);
}

} // namespace scwx::qt::util::color
