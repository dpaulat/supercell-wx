#pragma once

#include <memory>
#include <string>
#include <unordered_map>
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
std::unordered_map<std::string, std::string>
GetCounties(const std::string& state);
const std::unordered_map<std::string, std::string>& GetStates();
const std::unordered_map<std::string, std::string>& GetWFOs();
const std::string& GetWFOName(const std::string& wfoId);

} // namespace CountyDatabase
} // namespace config
} // namespace qt
} // namespace scwx
