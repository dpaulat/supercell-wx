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
   {Hotkey::SelectLevel2Ref, "select_l2_ref"},
   {Hotkey::SelectLevel2Vel, "select_l2_vel"},
   {Hotkey::SelectLevel2SW, "select_l2_sw"},
   {Hotkey::SelectLevel2ZDR, "select_l2_zdr"},
   {Hotkey::SelectLevel2Phi, "select_l2_phi"},
   {Hotkey::SelectLevel2Rho, "select_l2_rho"},
   {Hotkey::SelectLevel2CFP, "select_l2_cfp"},
   {Hotkey::SelectLevel3Ref, "select_l3_ref"},
   {Hotkey::SelectLevel3Vel, "select_l3_vel"},
   {Hotkey::SelectLevel3SRM, "select_l3_srm"},
   {Hotkey::SelectLevel3SW, "select_l3_sw"},
   {Hotkey::SelectLevel3ZDR, "select_l3_zdr"},
   {Hotkey::SelectLevel3KDP, "select_l3_kdp"},
   {Hotkey::SelectLevel3CC, "select_l3_cc"},
   {Hotkey::SelectLevel3VIL, "select_l3_vil"},
   {Hotkey::SelectLevel3ET, "select_l3_et"},
   {Hotkey::SelectLevel3HC, "select_l3_hc"},
   {Hotkey::SelectLevel3Acc, "select_l3_acc"},
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
   {Hotkey::SelectLevel2Ref, "Select L2 REF"},
   {Hotkey::SelectLevel2Vel, "Select L2 VEL"},
   {Hotkey::SelectLevel2SW, "Select L2 SW"},
   {Hotkey::SelectLevel2ZDR, "Select L2 ZDR"},
   {Hotkey::SelectLevel2Phi, "Select L2 PHI"},
   {Hotkey::SelectLevel2Rho, "Select L2 RHO"},
   {Hotkey::SelectLevel2CFP, "Select L2 CFP"},
   {Hotkey::SelectLevel3Ref, "Select L3 REF"},
   {Hotkey::SelectLevel3Vel, "Select L3 VEL"},
   {Hotkey::SelectLevel3SRM, "Select L3 SRM"},
   {Hotkey::SelectLevel3SW, "Select L3 SW"},
   {Hotkey::SelectLevel3ZDR, "Select L3 ZDR"},
   {Hotkey::SelectLevel3KDP, "Select L3 KDP"},
   {Hotkey::SelectLevel3CC, "Select L3 CC"},
   {Hotkey::SelectLevel3VIL, "Select L3 VIL"},
   {Hotkey::SelectLevel3ET, "Select L3 ET"},
   {Hotkey::SelectLevel3HC, "Select L3 HC"},
   {Hotkey::SelectLevel3Acc, "Select L3 ACC"},
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
