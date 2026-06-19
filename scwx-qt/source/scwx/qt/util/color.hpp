#pragma once

#include <boost/gil/typedefs.hpp>
#include <imgui.h>

namespace scwx
{
namespace qt
{
namespace util
{
namespace color
{

/**
 * Blends two Boost.GIL 8-bit RGBA pixels
 *
 * @param foreground Foreground color
 * @param background Background color
 *
 * @return Blended color
 */
boost::gil::rgba8_pixel_t Blend(const boost::gil::rgba8_pixel_t& foreground,
                                const boost::gil::rgba8_pixel_t& background);

/**
 * Converts a Boost.GIL 8-bit RGBA pixel to an ARGB string used by Qt libraries.
 *
 * @param color RGBA8 pixel
 *
 * @return ARGB string in the format #AARRGGBB
 */
std::string ToArgbString(const boost::gil::rgba8_pixel_t& color);

/**
 * Converts a Boost.GIL 8-bit RGBA pixel to an ImVec4 structure used by ImGui.
 *
 * @param color RGBA8 pixel
 *
 * @return ImVec4 structure
 */
ImVec4 ToImVec4(const boost::gil::rgba8_pixel_t& color);

/**
 * Converts an ARGB string used by Qt libraries to an ImVec4 structure used by
 * ImGui.
 *
 * @param argbString ARGB string in the format #AARRGGBB
 *
 * @return ImVec4 structure
 */
ImVec4 ToImVec4(const std::string& argbString);

/**
 * Converts an ARGB string used by Qt libraries to a Boost.GIL 8-bit RGBA pixel.
 *
 * @param argbString ARGB string in the format #AARRGGBB
 *
 * @return RGBA8 pixel
 */
boost::gil::rgba8_pixel_t ToRgba8PixelT(const std::string& argbString);

/**
 * Converts an ARGB string used by Qt libraries to a Boost.GIL 32-bit RGBA
 * floating point pixel.
 *
 * @param argbString ARGB string in the format #AARRGGBB
 *
 * @return RGBA32 floating point pixel
 */
boost::gil::rgba32f_pixel_t ToRgba32fPixelT(const std::string& argbString);

/**
 * Validates an ARGB string used by Qt libraries.
 *
 * @param argbString
 *
 * @return Validity of ARGB string
 */
bool ValidateArgbString(const std::string& argbString);

} // namespace color
} // namespace util
} // namespace qt
} // namespace scwx
