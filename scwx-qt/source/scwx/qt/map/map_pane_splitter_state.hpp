#pragma once

#include <QList>

namespace scwx::qt::map
{

/**
 * @brief Returns true if every splitter size in the list is strictly positive.
 *
 * Used to reject persisted splitter state when a row or column is collapsed
 * (e.g. a pop-out placeholder with zero size) so equal pane sizing is used
 * instead.
 */
[[nodiscard]] inline bool
MapPaneSplitterStateSizesAllPositive(const QList<int>& sizes)
{
   if (sizes.isEmpty())
   {
      return false;
   }
   for (const int s : sizes)
   {
      if (s <= 0)
      {
         return false;
      }
   }
   return true;
}

} // namespace scwx::qt::map
