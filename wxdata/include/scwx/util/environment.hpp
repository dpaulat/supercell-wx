#pragma once

#include <string>

namespace scwx
{
namespace util
{

std::string GetEnvironment(const std::string& name);
void        SetEnvironment(const std::string& name, const std::string& value);

} // namespace util
} // namespace scwx
