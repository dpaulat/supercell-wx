#include <scwx/network/cpr.hpp>

namespace scwx
{
namespace network
{
namespace cpr
{

static const std::string logPrefix_ = "scwx::network::cpr";

static ::cpr::Header header_ {};

::cpr::Header GetHeader()
{
   return header_;
}

void SetUserAgent(const std::string& userAgent)
{
   header_.insert_or_assign("User-Agent", userAgent);
}

} // namespace cpr
} // namespace network
} // namespace scwx
