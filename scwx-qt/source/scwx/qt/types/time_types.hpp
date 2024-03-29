#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class ClockFormat
{
   _12Hour,
   _24Hour,
   Unknown
};
typedef scwx::util::
   Iterator<ClockFormat, ClockFormat::_12Hour, ClockFormat::_24Hour>
      ClockFormatIterator;

enum class DefaultTimeZone
{
   Local,
   Radar,
   UTC,
   Unknown
};
typedef scwx::util::
   Iterator<DefaultTimeZone, DefaultTimeZone::Local, DefaultTimeZone::UTC>
      DefaultTimeZoneIterator;

ClockFormat        GetClockFormat(const std::string& name);
const std::string& GetClockFormatName(ClockFormat clockFormat);
DefaultTimeZone    GetDefaultTimeZone(const std::string& name);
const std::string& GetDefaultTimeZoneName(DefaultTimeZone timeZone);

} // namespace types
} // namespace qt
} // namespace scwx
