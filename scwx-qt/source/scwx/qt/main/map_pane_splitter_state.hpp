#pragma once

#include <QList>

namespace scwx::qt::main
{

// Used when deciding whether to apply |map_pane_splitter_state|; rows/columns
// that include 0 (e.g. pop-out column collapsed) must be rejected in favor of
// equal sizes.
[[nodiscard]] inline bool
MapPaneSplitterStateSizesAllPositive(const QList<int>& sizes)
{
   if (sizes.isEmpty())
   {
      return false;
   }
   for (int s : sizes)
   {
      if (s <= 0)
      {
         return false;
      }
   }
   return true;
}

} // namespace scwx::qt::main
