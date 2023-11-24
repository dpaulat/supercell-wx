#include <scwx/qt/types/qt_types.hpp>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<UiStyle, std::string> uiStyleName_ {
   {UiStyle::Default, "Default"},
   {UiStyle::Fusion, "Fusion"},
   {UiStyle::Unknown, "?"}};

UiStyle GetUiStyle(const std::string& name)
{
   auto result =
      std::find_if(uiStyleName_.cbegin(),
                   uiStyleName_.cend(),
                   [&](const std::pair<UiStyle, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != uiStyleName_.cend())
   {
      return result->first;
   }
   else
   {
      return UiStyle::Unknown;
   }
}

std::string GetUiStyleName(UiStyle uiStyle)
{
   return uiStyleName_.at(uiStyle);
}

} // namespace types
} // namespace qt
} // namespace scwx
