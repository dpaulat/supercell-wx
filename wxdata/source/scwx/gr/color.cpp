#include <scwx/gr/color.hpp>

#include <limits>

#include <hsluv.h>

namespace scwx
{
namespace gr
{

template<typename T>
T RoundChannel(double value);
template<typename T>
T StringToDecimal(const std::string& str);

boost::gil::rgba8_pixel_t ParseColor(const std::vector<std::string>& tokenList,
                                     std::size_t                     startIndex,
                                     ColorMode                       colorMode,
                                     bool                            hasAlpha)
{

   std::uint8_t r {};
   std::uint8_t g {};
   std::uint8_t b {};
   std::uint8_t a = 255;

   if (colorMode == ColorMode::RGBA)
   {
      if (tokenList.size() >= startIndex + 3)
      {
         r = StringToDecimal<std::uint8_t>(tokenList[startIndex + 0]);
         g = StringToDecimal<std::uint8_t>(tokenList[startIndex + 1]);
         b = StringToDecimal<std::uint8_t>(tokenList[startIndex + 2]);
      }

      if (hasAlpha && tokenList.size() >= startIndex + 4)
      {
         a = StringToDecimal<std::uint8_t>(tokenList[startIndex + 3]);
      }
   }
   else // if (colorMode == ColorMode::HSLuv)
   {
      double h {};
      double s {};
      double l {};

      if (tokenList.size() >= startIndex + 3)
      {
         h = std::stod(tokenList[startIndex + 0]);
         s = std::stod(tokenList[startIndex + 1]);
         l = std::stod(tokenList[startIndex + 2]);
      }

      double dr;
      double dg;
      double db;

      hsluv2rgb(h, s, l, &dr, &dg, &db);

      r = RoundChannel<std::uint8_t>(dr * 255.0);
      g = RoundChannel<std::uint8_t>(dg * 255.0);
      b = RoundChannel<std::uint8_t>(db * 255.0);
   }

   return boost::gil::rgba8_pixel_t {r, g, b, a};
}

template<typename T>
T RoundChannel(double value)
{
   return static_cast<T>(std::clamp<int>(std::lround(value),
                                         std::numeric_limits<T>::min(),
                                         std::numeric_limits<T>::max()));
}

template<typename T>
T StringToDecimal(const std::string& str)
{
   return static_cast<T>(std::clamp<int>(std::stoi(str),
                                         std::numeric_limits<T>::min(),
                                         std::numeric_limits<T>::max()));
}

} // namespace gr
} // namespace scwx
