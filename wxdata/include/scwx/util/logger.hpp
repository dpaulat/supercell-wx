#pragma once

#include <memory>
#include <string>

#pragma warning(push, 0)
#include <spdlog/logger.h>
#pragma warning(pop)

namespace scwx
{
namespace util
{
namespace Logger
{

void                            Initialize();
std::shared_ptr<spdlog::logger> Create(const std::string& name);

} // namespace Logger
} // namespace util
} // namespace scwx
