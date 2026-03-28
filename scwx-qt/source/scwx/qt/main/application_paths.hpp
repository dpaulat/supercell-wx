#pragma once

#include <scwx/util/iterator.hpp>

#include <cstdint>
#include <filesystem>

namespace scwx::qt::main::ApplicationPaths
{

enum class StandardLocation : std::uint8_t
{
   Cache,
   FontCache,
   Local,
   Log,
   Pictures,
   Settings,
   Temp
};
using StandardLocationIterator = scwx::util::
   Iterator<StandardLocation, StandardLocation::Cache, StandardLocation::Temp>;

void Initialize();
void LogErrors();

/**
 * @brief Resets all stored location paths and error messages.
 * @note Primarily intended for testing purposes.
 */
void Reset();

[[nodiscard]] const std::filesystem::path& GetLocation(StandardLocation type);

} // namespace scwx::qt::main::ApplicationPaths
