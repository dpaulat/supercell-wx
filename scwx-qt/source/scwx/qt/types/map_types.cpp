#include <scwx/qt/types/map_types.hpp>

#include <unordered_map>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<MapTime, std::string> mapTimeName_ {
   {MapTime::Live, "Live"}, {MapTime::Archive, "Archive"}};

std::string GetMapTimeName(MapTime mapTime)
{
   return mapTimeName_.at(mapTime);
}

} // namespace types
} // namespace qt
} // namespace scwx
