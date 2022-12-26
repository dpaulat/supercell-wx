#pragma once

#include <boost/gil/typedefs.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace color
{

/**
 * Converts a Boost.GIL 8-bit RGBA pixel to an ARGB string used by Qt libraries.
 *
 * @param color RGBA8 pixel
 *
 * @return ARGB string in the format #AARRGGBB
 */
std::string ToArgbString(const boost::gil::rgba8_pixel_t& color);

/**
 * Converts an ARGB string used by Qt libraries to a Boost.GIL 8-bit RGBA pixel.
 *
 * @param argbString ARGB string in the format #AARRGGBB
 *
 * @return RGBA8 pixel
 */
boost::gil::rgba8_pixel_t ToRgba8PixelT(const std::string& argbString);

} // namespace color
} // namespace util
} // namespace qt
} // namespace scwx
