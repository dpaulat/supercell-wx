#pragma once

#include <cpr/cprtypes.h>

namespace scwx
{
namespace network
{
namespace cpr
{

::cpr::Header GetHeader();
void          SetUserAgent(const std::string& userAgent);

} // namespace cpr
} // namespace network
} // namespace scwx
