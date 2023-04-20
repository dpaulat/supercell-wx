#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace scwx
{
namespace network
{

struct DirListRecord
{
   std::string                filename_ = {};
   std::filesystem::file_type type_     = std::filesystem::file_type::unknown;
   std::chrono::system_clock::time_point mtime_ =
      {};             ///< Modified time (server time)
   size_t size_ = 0u; ///< Approximate file size in bytes
};

/**
 * @brief Retrieve Directory Listing
 *
 * Retrieves a directory listing. Supports default Apache-style directory
 * listings only.
 */
std::vector<DirListRecord> DirList(const std::string& baseUrl);

} // namespace network
} // namespace scwx
