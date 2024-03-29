#pragma once

#include <scwx/util/iterator.hpp>

#include <chrono>
#include <optional>
#include <string>

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

enum class ClockFormat
{
   _12Hour,
   _24Hour,
   Unknown
};
typedef scwx::util::
   Iterator<ClockFormat, ClockFormat::_12Hour, ClockFormat::_24Hour>
      ClockFormatIterator;

ClockFormat        GetClockFormat(const std::string& name);
const std::string& GetClockFormatName(ClockFormat clockFormat);

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
