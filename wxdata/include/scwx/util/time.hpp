#pragma once

#include <chrono>
#include <optional>

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

template<typename T>
std::optional<std::chrono::sys_time<T>>
TryParseDateTime(const std::string& dateTimeFormat, const std::string& str);

} // namespace util
} // namespace scwx
