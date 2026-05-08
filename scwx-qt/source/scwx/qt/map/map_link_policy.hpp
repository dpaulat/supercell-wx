#pragma once

#include <cstddef>

namespace scwx::qt::map
{
class MapWidget;

/**
 * @brief Whether linked map-parameter sync should run for the current call.
 */
[[nodiscard]] bool
ShouldApplyLinkedMapParameterSync(std::size_t      mapCount,
                                  const MapWidget* signalSource) noexcept;

} // namespace scwx::qt::map
