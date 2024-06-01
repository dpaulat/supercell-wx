#pragma once

#include <memory>
#include <string>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <spdlog/logger.h>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace util
{
namespace Logger
{

void                            Initialize();
void                            AddFileSink(const std::string& baseFilename);
std::shared_ptr<spdlog::logger> Create(const std::string& name);

} // namespace Logger
} // namespace util
} // namespace scwx
