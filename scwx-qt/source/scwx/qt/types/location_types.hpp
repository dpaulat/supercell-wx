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
   Unknown
};
typedef scwx::util::
   Iterator<LocationMethod, LocationMethod::Fixed, LocationMethod::County>
      LocationMethodIterator;

LocationMethod     GetLocationMethod(const std::string& name);
const std::string& GetLocationMethodName(LocationMethod locationMethod);

} // namespace types
} // namespace qt
} // namespace scwx
