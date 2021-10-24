#pragma once

#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace json
{

bool FromJsonString(const boost::json::object& json,
                    const std::string&         key,
                    std::string&               value,
                    const std::string&         defaultValue);

boost::json::value ReadJsonFile(const std::string& path);
void               WriteJsonFile(const std::string&        path,
                                 const boost::json::value& json,
                                 bool                      prettyPrint = true);

} // namespace json
} // namespace util
} // namespace qt
} // namespace scwx
