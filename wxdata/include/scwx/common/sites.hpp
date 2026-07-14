#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace scwx::common
{

/**
 * @brief Get the canonical radar ID from the radar ID.
 *
 * The canonical radar ID is the radar ID that is used to identify the radar
 * site. It is typically the same as the radar ID, but may be different in some
 * cases. For example, the canonical radar ID for "TPBI" is "TDJT".
 *
 * @param radarId The radar ID.
 * @return The canonical radar ID.
 */
std::string GetCanonicalRadarId(const std::string& radarId);

/**
 * @brief Get the site ID from the radar ID.
 *
 * @param radarId The radar ID.
 * @return The site ID.
 */
std::string GetSiteId(const std::string& radarId);

/**
 * @brief Get the site ID map.
 *
 * The site ID map is a static map of site IDs to canonical radar IDs based on
 * the radar ID transition rules.
 *
 * @return The site ID map.
 */
std::unordered_map<std::string, std::string> GetSiteIdMap();

/**
 * @brief Get ordered radar ID candidates from the radar ID and date.
 *
 * For sites with a rename transition (e.g. TPBI → TDJT), returns an ordered
 * list used for first-hit-wins lookups. The transition window is cutover
 * uncertainty (when NWS may flip the ID), not a period of dual publication:
 * prefer the canonical ID once it has data and disregard the legacy ID.
 *
 * Ordering is canonical first when the date is on/after transition start,
 * then the original ID while the date is before transition end. Callers
 * should try candidates in order and stop at the first with usable data.
 *
 * @param radarId The radar ID (original or canonical).
 * @param date The request date (UTC). Unspecified selects both candidates.
 * @return The radar ID candidates in try order.
 */
std::vector<std::string>
GetRadarIdCandidates(const std::string&                          radarId,
                     const std::chrono::system_clock::time_point date = {});

} // namespace scwx::common
