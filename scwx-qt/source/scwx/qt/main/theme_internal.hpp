#pragma once

#include <scwx/qt/types/qt_types.hpp>

#include <string>
#include <vector>

namespace scwx::qt::main::internal
{

[[nodiscard]] bool HasStyleArgument(const std::vector<std::string>& args);

/**
 * Whether the UI style takes its light/dark appearance from the operating
 * system (no forced color scheme and no fixed palette file).
 */
[[nodiscard]] bool FollowsSystemColorScheme(types::UiStyle uiStyle);

} // namespace scwx::qt::main::internal
