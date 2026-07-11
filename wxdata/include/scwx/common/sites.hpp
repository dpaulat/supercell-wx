#pragma once

#include <chrono>
#include <string>
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
 * @brief Get ordered radar ID candidates from the radar ID and date.
 *
 * @param radarId The radar ID.
 * @param date The date.
 * @return The radar ID candidates.
 */
std::vector<std::string>
GetRadarIdCandidates(const std::string&                          radarId,
                     const std::chrono::system_clock::time_point date);

} // namespace scwx::common
