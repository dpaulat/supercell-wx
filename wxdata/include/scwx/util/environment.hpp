#pragma once

#include <string>

namespace scwx
{
namespace util
{

std::string GetEnvironment(const std::string& name);
void        SetEnvironment(const std::string& name, const std::string& value);
void        UnsetEnvironment(const std::string& name);

bool HasEnvironment(const std::string& name);
bool IsEnvironmentEnabled(const std::string& name,
                          bool               defaultWhenUnset = true);

} // namespace util
} // namespace scwx
