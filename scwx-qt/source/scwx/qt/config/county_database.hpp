#pragma once

#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace qt
{
namespace config
{
namespace CountyDatabase
{

void        Initialize();
std::string GetCountyName(const std::string& id);

} // namespace CountyDatabase
} // namespace config
} // namespace qt
} // namespace scwx
