#pragma once

#include <optional>

#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace json
{

bool FromJsonInt64(const boost::json::object& json,
                   const std::string&         key,
                   int64_t&                   value,
                   const int64_t              defaultValue,
                   std::optional<int64_t>     minValue,
                   std::optional<int64_t>     maxValue);
bool FromJsonString(const boost::json::object& json,
                    const std::string&         key,
                    std::string&               value,
                    const std::string&         defaultValue,
                    size_t                     minLength = 0);

boost::json::value ReadJsonFile(const std::string& path);
void               WriteJsonFile(const std::string&        path,
                                 const boost::json::value& json,
                                 bool                      prettyPrint = true);

} // namespace json
} // namespace util
} // namespace qt
} // namespace scwx
