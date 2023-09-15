#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

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

TooltipMethod GetTooltipMethod(const std::string& name);
std::string   GetTooltipMethodName(TooltipMethod tooltipMethod);

} // namespace types
} // namespace qt
} // namespace scwx
