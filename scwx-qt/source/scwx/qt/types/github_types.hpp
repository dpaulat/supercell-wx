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
 * @brief GitHub Release Asset object
 *
 * <https://docs.github.com/en/rest/releases/assets?apiVersion=2022-11-28>
 */
struct ReleaseAsset
{
   std::string name_ {};
   std::string contentType_ {};
   std::string browserDownloadUrl_ {};
};

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

   std::vector<ReleaseAsset> assets_ {};
};

ReleaseAsset tag_invoke(boost::json::value_to_tag<ReleaseAsset>,
                        const boost::json::value& jv);
Release      tag_invoke(boost::json::value_to_tag<Release>,
                        const boost::json::value& jv);

} // namespace gh
} // namespace types
} // namespace qt
} // namespace scwx
