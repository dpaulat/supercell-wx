#include <scwx/qt/map/map_link_policy.hpp>

namespace scwx::qt::map
{

bool ShouldApplyLinkedMapParameterSync(std::size_t      mapCount,
                                       const MapWidget* signalSource) noexcept
{
   if (mapCount <= 1u)
   {
      return true;
   }
   return signalSource != nullptr;
}

} // namespace scwx::qt::map
