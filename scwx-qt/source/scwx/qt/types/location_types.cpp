#include <scwx/qt/types/location_types.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<LocationMethod, std::string>
   locationMethodName_ {{LocationMethod::Fixed, "Fixed"},
                        {LocationMethod::Track, "Track"},
                        {LocationMethod::Unknown, "?"}};

SCWX_GET_ENUM(LocationMethod, GetLocationMethod, locationMethodName_)

const std::string& GetLocationMethodName(LocationMethod locationMethod)
{
   return locationMethodName_.at(locationMethod);
}

} // namespace types
} // namespace qt
} // namespace scwx
