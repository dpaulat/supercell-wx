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
   Log,
   Pictures,
   Settings,
   Temp
};
using StandardLocationIterator =
   scwx::util::Iterator<StandardLocation,
                        StandardLocation::Cache,
                        StandardLocation::Temp>;

void Initialize();
void LogErrors();

[[nodiscard]] const std::filesystem::path& GetLocation(StandardLocation type);

} // namespace scwx::qt::main::ApplicationPaths
