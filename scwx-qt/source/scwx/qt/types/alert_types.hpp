#pragma once

#include <scwx/awips/phenomenon.hpp>
#include <scwx/util/iterator.hpp>

#include <string>
#include <vector>

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

const std::vector<awips::Phenomenon>& GetAlertAudioPhenomena();

} // namespace types
} // namespace qt
} // namespace scwx
