#pragma once

#include <cstddef>

namespace scwx::qt::map
{
class MapWidget;
}

namespace scwx::qt::main
{

// Linked panes: MapParametersChanged must come from a real map signal when
// N>1, otherwise update would apply to every pane (and duplicate work on
// the leader). N<=1: caller may invoke without a sender; sync is a no-op.
[[nodiscard]] inline bool ShouldApplyLinkedMapParameterSync(
   std::size_t mapCount, const scwx::qt::map::MapWidget* signalSource) noexcept
{
   if (mapCount <= 1u)
   {
      return true;
   }
   return signalSource != nullptr;
}

} // namespace scwx::qt::main
