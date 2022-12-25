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

std::string ToArgbString(const boost::gil::rgba8_pixel_t& color);

} // namespace color
} // namespace util
} // namespace qt
} // namespace scwx
