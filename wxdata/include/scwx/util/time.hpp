#pragma once

#include <chrono>

#if !defined(_MSC_VER)
#   include <date/tz.h>
#endif

namespace scwx
{
namespace util
{

#if defined(_MSC_VER)
typedef std::chrono::time_zone time_zone;
#else
typedef date::time_zone time_zone;
#endif

std::chrono::system_clock::time_point TimePoint(uint32_t modifiedJulianDate,
                                                uint32_t milliseconds);

std::string TimeString(std::chrono::system_clock::time_point time,
                       const time_zone*                      timeZone = nullptr,
                       bool                                  epochValid = true);

} // namespace util
} // namespace scwx
