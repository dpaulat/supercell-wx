#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class AlertAction
{
   Go,
   View,
   Unknown
};
typedef scwx::util::Iterator<AlertAction, AlertAction::Go, AlertAction::View>
   AlertActionIterator;

AlertAction GetAlertAction(const std::string& name);
std::string GetAlertActionName(AlertAction alertAction);

} // namespace types
} // namespace qt
} // namespace scwx
