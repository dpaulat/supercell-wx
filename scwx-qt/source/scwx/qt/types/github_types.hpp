#pragma once

#include <string>

#include <boost/json/value.hpp>

namespace scwx
{
namespace qt
{
namespace types
{
namespace gh
{

/**
 * @brief GitHub Release object
 *
 * <https://docs.github.com/en/rest/releases/releases?apiVersion=2022-11-28>
 */
struct Release
{
   std::string name_ {};
   std::string htmlUrl_ {};
   std::string body_ {};
   bool        draft_ {};
   bool        prerelease_ {};
};

Release tag_invoke(boost::json::value_to_tag<Release>,
                   const boost::json::value& jv);

} // namespace gh
} // namespace types
} // namespace qt
} // namespace scwx
