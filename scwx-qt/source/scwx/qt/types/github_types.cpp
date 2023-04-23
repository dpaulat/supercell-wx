#include <scwx/qt/types/github_types.hpp>

#include <boost/json/value_to.hpp>

namespace scwx
{
namespace qt
{
namespace types
{
namespace gh
{

Release tag_invoke(boost::json::value_to_tag<Release>,
                   const boost::json::value& jv)
{
   auto jo = jv.as_object();

   Release release {};

   // Required parameters
   release.name_       = jo.at("name").as_string();
   release.htmlUrl_    = jo.at("html_url").as_string();
   release.draft_      = jo.at("draft").as_bool();
   release.prerelease_ = jo.at("prerelease").as_bool();

   // Optional parameters
   if (jo.contains("body"))
   {
      release.body_ = jo.at("body").as_string();
   }

   return release;
}

} // namespace gh
} // namespace types
} // namespace qt
} // namespace scwx
