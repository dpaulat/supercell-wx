#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scwx::qt::map
{

/**
 * @brief Parses persisted map pane view-link state.
 *
 * @param json JSON encoded "map_pane_view_link_state" value.
 * @param expectedGw Expected map grid width.
 * @param expectedGh Expected map grid height.
 *
 * @return Parsed link flags, or std::nullopt when the JSON is invalid, grid
 * dimensions mismatch, length is invalid, or an element cannot be converted to
 * a boolean. Linked elements may be bool, 0/1, or "true"/"false" / "0"/"1"
 * strings.
 */
[[nodiscard]] std::optional<std::vector<bool>> TryParseMapPaneViewLinkStateJson(
   const std::string& json, int64_t expectedGw, int64_t expectedGh);

/**
 * @brief Serializes map pane view-link flags.
 *
 * @param gw Map grid width.
 * @param gh Map grid height.
 * @param linked Link flags in row-major map pane order.
 *
 * @return Compact JSON string, or an empty string if inputs are invalid.
 */
[[nodiscard]] std::string SerializeMapPaneViewLinkStateJson(
   int64_t gw, int64_t gh, const std::vector<bool>& linked);

} // namespace scwx::qt::map
