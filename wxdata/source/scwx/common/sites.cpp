#include <scwx/common/products.hpp>

#include <unordered_map>

namespace scwx
{
namespace common
{

std::string GetSiteId(const std::string& radarId)
{
   size_t      siteIdIndex = std::max<size_t>(radarId.length(), 3u) - 3u;
   std::string siteId      = radarId.substr(siteIdIndex);
   return siteId;
}

} // namespace common
} // namespace scwx
