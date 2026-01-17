#pragma once

#include <scwx/util/iterator.hpp>

#include <chrono>
#include <optional>
#include <string>

#include <boost/signals2/signal.hpp>

#if (__cpp_lib_chrono < 201907L)
#   include <date/tz.h>
#endif

namespace scwx::util::time
{

#if (__cpp_lib_chrono >= 201907L)
using time_zone = std::chrono::time_zone;
#else
using time_zone = date::time_zone;
#endif

enum class ClockFormat : std::uint8_t
{
   _12Hour,
   _24Hour,
   Default,
   Unknown
};
using ClockFormatIterator = scwx::util::
   Iterator<ClockFormat, ClockFormat::_12Hour, ClockFormat::_24Hour>;
using ClockFormatAllIterator = scwx::util::
   Iterator<ClockFormat, ClockFormat::_12Hour, ClockFormat::Default>;

ClockFormat        GetClockFormat(const std::string& name);
const std::string& GetClockFormatName(ClockFormat clockFormat);

template<typename Clock = std::chrono::system_clock>
std::chrono::time_point<Clock> now();

const time_zone* current_time_zone();
void             set_current_time_zone(const time_zone* timeZone);
boost::signals2::signal<void(const time_zone*)>&
current_time_zone_changed_signal();

ClockFormat default_clock_format();
void        set_default_clock_format(ClockFormat clockFormat);
boost::signals2::signal<void(ClockFormat)>&
                                      default_clock_format_changed_signal();
std::chrono::system_clock::time_point TimePoint(uint32_t modifiedJulianDate,
                                                uint32_t milliseconds);

std::string TimeString(std::chrono::system_clock::time_point time,
                       ClockFormat      clockFormat = ClockFormat::_24Hour,
                       const time_zone* timeZone    = nullptr,
                       bool             epochValid  = true);

template<typename T>
std::optional<std::chrono::sys_time<T>>
TryParseDateTime(const std::string& dateTimeFormat, const std::string& str);

} // namespace scwx::util::time

namespace scwx::util
{
// Add types and functions to scwx::util for compatibility
using time::ClockFormat;
using time::ClockFormatIterator;
using time::GetClockFormat;
using time::GetClockFormatName;
using time::time_zone;
using time::TimePoint;
using time::TimeString;
using time::TryParseDateTime;
} // namespace scwx::util
