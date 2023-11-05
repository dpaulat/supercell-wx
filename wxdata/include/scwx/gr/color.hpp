#pragma once

#include <scwx/gr/gr_types.hpp>

#include <string>
#include <vector>

#include <boost/gil/typedefs.hpp>

namespace scwx
{
namespace gr
{

boost::gil::rgba8_pixel_t ParseColor(const std::vector<std::string>& tokenList,
                                     std::size_t                     startIndex,
                                     ColorMode                       colorMode,
                                     bool hasAlpha = true);

} // namespace gr
} // namespace scwx
