#pragma once

#include <optional>

#include <boost/json/value.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace json
{

boost::json::value ReadJsonFile(const std::string& path);
void               WriteJsonFile(const std::string&        path,
                                 const boost::json::value& json,
                                 bool                      prettyPrint = true);

} // namespace json
} // namespace util
} // namespace qt
} // namespace scwx
