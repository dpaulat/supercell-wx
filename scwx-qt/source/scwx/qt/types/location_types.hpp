#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class LocationMethod
{
   Fixed,
   Track,
   County,
   All,
   Unknown
};
typedef scwx::util::
   Iterator<LocationMethod, LocationMethod::Fixed, LocationMethod::All>
      LocationMethodIterator;

LocationMethod     GetLocationMethod(const std::string& name);
const std::string& GetLocationMethodName(LocationMethod locationMethod);

} // namespace types
} // namespace qt
} // namespace scwx
