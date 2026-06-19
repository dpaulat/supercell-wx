#pragma once

#include <boost/json/value.hpp>

namespace scwx::util::json
{

boost::json::value ReadJsonFile(const std::string& path);
boost::json::value ReadJsonStream(std::istream& is);
boost::json::value ReadJsonString(std::string_view sv);
void               WriteJsonFile(const std::string&        path,
                                 const boost::json::value& json,
                                 bool                      prettyPrint = true);
void               WriteJsonStream(std::ostream&             os,
                                   const boost::json::value& json,
                                   bool                      prettyPrint = true);

} // namespace scwx::util::json
