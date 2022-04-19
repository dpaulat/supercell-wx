#pragma once

#include <memory>
#include <string>

#include <spdlog/logger.h>

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
