#pragma once

#include <units/core.h>

namespace units
{

namespace dimension
{

struct font_size_tag
{
   static constexpr const char* const name         = "font size";
   static constexpr const char* const abbreviation = "px";
};

using font_size = make_dimension<font_size_tag>;

} // namespace dimension

UNIT_ADD(font_size,
         pixels,
         px,
         conversion_factor<std::ratio<1>, dimension::font_size>)
UNIT_ADD(font_size, points, pt, conversion_factor<std::ratio<4, 3>, pixels<>>)

} // namespace units

namespace scwx
{
namespace qt
{
namespace types
{

enum class Font
{
   din1451alt,
   din1451alt_g,
   Inconsolata_Regular
};

} // namespace types
} // namespace qt
} // namespace scwx
