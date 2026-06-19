#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class Hotkey
{
   AddLocationMarker,
   ChangeMapStyle,
   CopyCursorCoordinates,
   CopyMapCoordinates,
   MapPanUp,
   MapPanDown,
   MapPanLeft,
   MapPanRight,
   MapRotateClockwise,
   MapRotateCounterclockwise,
   MapZoomIn,
   MapZoomOut,
   ProductCategoryPrevious,
   ProductCategoryNext,
   ProductTiltDecrease,
   ProductTiltIncrease,
   ScreenCaptureCopy,
   ScreenCaptureSaveImage,
   SelectLevel2Ref,
   SelectLevel2Vel,
   SelectLevel2SW,
   SelectLevel2ZDR,
   SelectLevel2Phi,
   SelectLevel2Rho,
   SelectLevel2CFP,
   SelectLevel3Ref,
   SelectLevel3Vel,
   SelectLevel3SRM,
   SelectLevel3SW,
   SelectLevel3ZDR,
   SelectLevel3KDP,
   SelectLevel3CC,
   SelectLevel3VIL,
   SelectLevel3ET,
   SelectLevel3HC,
   SelectLevel3Acc,
   TimelineStepBegin,
   TimelineStepBack,
   TimelinePlay,
   TimelineStepNext,
   TimelineStepEnd,
   ToggleFullScreen,
   Unknown
};
typedef scwx::util::
   Iterator<Hotkey, Hotkey::AddLocationMarker, Hotkey::ToggleFullScreen>
      HotkeyIterator;

Hotkey             GetHotkeyFromShortName(const std::string& name);
Hotkey             GetHotkeyFromLongName(const std::string& name);
const std::string& GetHotkeyShortName(Hotkey hotkey);
const std::string& GetHotkeyLongName(Hotkey hotkey);

} // namespace types
} // namespace qt
} // namespace scwx
