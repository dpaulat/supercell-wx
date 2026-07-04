#include <scwx/common/sites.hpp>
#include <scwx/common/products.hpp>

#include <algorithm>

namespace scwx::common
{

std::string GetSiteId(const std::string& radarId)
{
   std::string siteId = radarId;

   // Shorten only if radarId does not contain digits
   if (!std::ranges::any_of(radarId, isdigit))
   {
      const std::size_t siteIdIndex =
         std::max<std::size_t>(radarId.length(), 3u) - 3u;
      siteId = radarId.substr(siteIdIndex);
   }

   return siteId;
}

} // namespace scwx::common
