#include <scwx/qt/types/alert_types.hpp>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<AlertAction, std::string> alertActionName_ {
   {AlertAction::Go, "Go"},
   {AlertAction::View, "View"},
   {AlertAction::Unknown, "?"}};

AlertAction GetAlertAction(const std::string& name)
{
   auto result =
      std::find_if(alertActionName_.cbegin(),
                   alertActionName_.cend(),
                   [&](const std::pair<AlertAction, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != alertActionName_.cend())
   {
      return result->first;
   }
   else
   {
      return AlertAction::Unknown;
   }
}

std::string GetAlertActionName(AlertAction alertAction)
{
   return alertActionName_.at(alertAction);
}

} // namespace types
} // namespace qt
} // namespace scwx
