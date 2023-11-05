#include <scwx/qt/types/text_types.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<FontCategory, std::string> fontCategoryName_ {
   {FontCategory::Default, "Default"},
   {FontCategory::Tooltip, "Tooltip"},
   {FontCategory::Unknown, "?"}};

static const std::unordered_map<TooltipMethod, std::string> tooltipMethodName_ {
   {TooltipMethod::ImGui, "ImGui"},
   {TooltipMethod::QToolTip, "Native Tooltip"},
   {TooltipMethod::QLabel, "Floating Label"},
   {TooltipMethod::Unknown, "?"}};

FontCategory GetFontCategory(const std::string& name)
{
   auto result =
      std::find_if(fontCategoryName_.cbegin(),
                   fontCategoryName_.cend(),
                   [&](const std::pair<FontCategory, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != fontCategoryName_.cend())
   {
      return result->first;
   }
   else
   {
      return FontCategory::Unknown;
   }
}

std::string GetFontCategoryName(FontCategory fontCategory)
{
   return fontCategoryName_.at(fontCategory);
}

TooltipMethod GetTooltipMethod(const std::string& name)
{
   auto result = std::find_if(
      tooltipMethodName_.cbegin(),
      tooltipMethodName_.cend(),
      [&](const std::pair<TooltipMethod, std::string>& pair) -> bool
      { return boost::iequals(pair.second, name); });

   if (result != tooltipMethodName_.cend())
   {
      return result->first;
   }
   else
   {
      return TooltipMethod::Unknown;
   }
}

std::string GetTooltipMethodName(TooltipMethod tooltipMethod)
{
   return tooltipMethodName_.at(tooltipMethod);
}

} // namespace types
} // namespace qt
} // namespace scwx
