#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class FontCategory
{
   Default,
   Tooltip,
   Unknown
};
typedef scwx::util::
   Iterator<FontCategory, FontCategory::Default, FontCategory::Tooltip>
      FontCategoryIterator;

enum class TooltipMethod
{
   ImGui,
   QToolTip,
   QLabel,
   Unknown
};
typedef scwx::util::
   Iterator<TooltipMethod, TooltipMethod::ImGui, TooltipMethod::QLabel>
      TooltipMethodIterator;

FontCategory  GetFontCategory(const std::string& name);
std::string   GetFontCategoryName(FontCategory fontCategory);
TooltipMethod GetTooltipMethod(const std::string& name);
std::string   GetTooltipMethodName(TooltipMethod tooltipMethod);

} // namespace types
} // namespace qt
} // namespace scwx
