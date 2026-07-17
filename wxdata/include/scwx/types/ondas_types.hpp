#pragma once

#include <string>
#include <vector>

namespace scwx::types::ondas
{

struct OndasDirListRecord
{
   std::string filename_ {};
   std::size_t size_ = 0; // 0 if not present in listing
};

/**
 * @brief Parse ONDAS dir.list content
 *
 * Parses the plain-text ONDAS directory listing format.
 * Each line is either:
 *   - "filename" (no size)
 *   - "size filename" (size in bytes, space-separated)
 *
 * Files are listed oldest-first per ONDAS spec.
 *
 * @param content The dir.list file content
 * @return Vector of records, ordered oldest to newest
 */
std::vector<OndasDirListRecord> ParseOndasDirList(const std::string& content);

} // namespace scwx::types::ondas
