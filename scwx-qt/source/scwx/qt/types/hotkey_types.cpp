#include <scwx/qt/types/hotkey_types.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<Hotkey, std::string> hotkeyShortName_ {
   {Hotkey::ChangeMapStyle, "change_map_style"},
   {Hotkey::CopyCursorCoordinates, "copy_cursor_coordinates"},
   {Hotkey::CopyMapCoordinates, "copy_map_coordinates"},
   {Hotkey::MapPanUp, "map_pan_up"},
   {Hotkey::MapPanDown, "map_pan_down"},
   {Hotkey::MapPanLeft, "map_pan_left"},
   {Hotkey::MapPanRight, "map_pan_right"},
   {Hotkey::MapRotateClockwise, "map_rotate_clockwise"},
   {Hotkey::MapRotateCounterclockwise, "map_rotate_counterclockwise"},
   {Hotkey::MapZoomIn, "map_zoom_in"},
   {Hotkey::MapZoomOut, "map_zoom_out"},
   {Hotkey::ProductTiltDecrease, "product_tilt_decrease"},
   {Hotkey::ProductTiltIncrease, "product_tilt_increase"},
   {Hotkey::TimelineStepBegin, "timeline_step_begin"},
   {Hotkey::TimelineStepBack, "timeline_step_back"},
   {Hotkey::TimelinePlay, "timeline_play"},
   {Hotkey::TimelineStepNext, "timeline_step_next"},
   {Hotkey::TimelineStepEnd, "timeline_step_end"},
   {Hotkey::Unknown, "?"}};

static const std::unordered_map<Hotkey, std::string> hotkeyLongName_ {
   {Hotkey::ChangeMapStyle, "Change Map Style"},
   {Hotkey::CopyCursorCoordinates, "Copy Cursor Coordinates"},
   {Hotkey::CopyMapCoordinates, "Copy Map Coordinates"},
   {Hotkey::MapPanUp, "Map Pan Up"},
   {Hotkey::MapPanDown, "Map Pan Down"},
   {Hotkey::MapPanLeft, "Map Pan Left"},
   {Hotkey::MapPanRight, "Map Pan Right"},
   {Hotkey::MapRotateClockwise, "Map Rotate Clockwise"},
   {Hotkey::MapRotateCounterclockwise, "Map Rotate Counterclockwise"},
   {Hotkey::MapZoomIn, "Map Zoom In"},
   {Hotkey::MapZoomOut, "Map Zoom Out"},
   {Hotkey::ProductTiltDecrease, "Product Tilt Decrease"},
   {Hotkey::ProductTiltIncrease, "Product Tilt Increase"},
   {Hotkey::TimelineStepBegin, "Timeline Step Begin"},
   {Hotkey::TimelineStepBack, "Timeline Step Back"},
   {Hotkey::TimelinePlay, "Timeline Play/Pause"},
   {Hotkey::TimelineStepNext, "Timeline Step Next"},
   {Hotkey::TimelineStepEnd, "Timeline Step End"},
   {Hotkey::Unknown, "?"}};

SCWX_GET_ENUM(Hotkey, GetHotkeyFromShortName, hotkeyShortName_)
SCWX_GET_ENUM(Hotkey, GetHotkeyFromLongName, hotkeyLongName_)

const std::string& GetHotkeyShortName(Hotkey hotkey)
{
   return hotkeyShortName_.at(hotkey);
}

const std::string& GetHotkeyLongName(Hotkey hotkey)
{
   return hotkeyLongName_.at(hotkey);
}

} // namespace types
} // namespace qt
} // namespace scwx
