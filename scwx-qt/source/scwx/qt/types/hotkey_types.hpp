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
   ProductTiltDecrease,
   ProductTiltIncrease,
   TimelineStepBegin,
   TimelineStepBack,
   TimelinePlay,
   TimelineStepNext,
   TimelineStepEnd,
   Unknown
};
typedef scwx::util::
   Iterator<Hotkey, Hotkey::ChangeMapStyle, Hotkey::TimelineStepEnd>
      HotkeyIterator;

Hotkey             GetHotkeyFromShortName(const std::string& name);
Hotkey             GetHotkeyFromLongName(const std::string& name);
const std::string& GetHotkeyShortName(Hotkey hotkey);
const std::string& GetHotkeyLongName(Hotkey hotkey);

} // namespace types
} // namespace qt
} // namespace scwx
