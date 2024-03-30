#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

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

DefaultTimeZone    GetDefaultTimeZone(const std::string& name);
const std::string& GetDefaultTimeZoneName(DefaultTimeZone timeZone);

} // namespace types
} // namespace qt
} // namespace scwx
