#pragma once

#include <string>

namespace scwx
{
namespace qt
{
namespace util
{
namespace network
{

/**
 * @brief Converts a local or remote URL to a consistent format.
 *
 * @param [in] urlString URL to normalize
 *
 * @return Normalized URL string
 */
std::string NormalizeUrl(const std::string& urlString);

} // namespace network
} // namespace util
} // namespace qt
} // namespace scwx
