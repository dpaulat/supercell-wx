#include <scwx/qt/types/texture_types.hpp>

#include <unordered_map>

namespace scwx
{
namespace qt
{
namespace types
{

struct TextureInfo
{
   std::string name_ {};
   std::string path_ {};
};

static const std::unordered_map<ImageTexture, TextureInfo> imageTextureInfo_ {
   {ImageTexture::CardinalPoint24,
    {"images/cardinal-point-24", ":/res/icons/flaticon/cardinal-point-24.png"}},
   {ImageTexture::Compass24,
    {"images/compass-24", ":/res/icons/flaticon/compass-24.png"}},
   {ImageTexture::Crosshairs24,
    {"images/crosshairs-24", ":/res/textures/images/crosshairs-24.png"}},
   {ImageTexture::Cursor17,
    {"images/cursor-17", ":/res/textures/images/cursor-17.png"}},
   {ImageTexture::Dot3, {"images/dot-3", ":/res/textures/images/dot.svg"}},
   {ImageTexture::LocationBriefcase,
    {"images/location-briefcase",
     ":/res/icons/font-awesome-6/briefcase-solid.svg"}},
   {ImageTexture::LocationBuildingColumns,
    {"images/location-building-columns",
     ":/res/icons/font-awesome-6/building-columns-solid.svg"}},
   {ImageTexture::LocationBuilding,
    {"images/location-building",
     ":/res/icons/font-awesome-6/building-solid.svg"}},
   {ImageTexture::LocationCaravan,
    {"images/location-caravan",
     ":/res/icons/font-awesome-6/caravan-solid.svg"}},
   {ImageTexture::LocationCrosshair,
    {"images/location-crosshair",
     ":/res/icons/font-awesome-6/location-crosshairs-solid.svg"}},
   {ImageTexture::LocationHouse,
    {"images/location-house",
     ":/res/icons/font-awesome-6/house-solid-white.svg"}},
   {ImageTexture::LocationMarker,
    {"images/location-marker", ":/res/textures/images/location-marker.svg"}},
   {ImageTexture::LocationPin,
    {"images/location-pin", ":/res/icons/font-awesome-6/location-pin.svg"}},
   {ImageTexture::LocationStar,
    {"images/location-star",
     ":/res/icons/font-awesome-6/star-solid-white.svg"}},
   {ImageTexture::LocationTent,
    {"images/location-tent", ":/res/icons/font-awesome-6/tent-solid.svg"}},
   {ImageTexture::MapboxLogo,
    {"images/mapbox-logo", ":/res/textures/images/mapbox-logo.svg"}},
   {ImageTexture::MapTilerLogo,
    {"images/maptiler-logo", ":/res/textures/images/maptiler-logo.svg"}},
   {ImageTexture::OpenFreeMapLogo,
    {"images/openfreemap-logo", ":res/textures/images/openfreemap-logo.jpg"}}};

static const std::unordered_map<LineTexture, TextureInfo> lineTextureInfo_ {
   {LineTexture::Default1x7,
    {"lines/default-1x7", ":/res/textures/lines/default-1x7.png"}},
   {LineTexture::TestPattern,
    {"lines/test-pattern", ":/res/textures/lines/test-pattern.png"}}};

const std::string& GetTextureName(ImageTexture imageTexture)
{ return imageTextureInfo_.at(imageTexture).name_; }

const std::string& GetTextureName(LineTexture lineTexture)
{ return lineTextureInfo_.at(lineTexture).name_; }

const std::string& GetTexturePath(ImageTexture imageTexture)
{ return imageTextureInfo_.at(imageTexture).path_; }

const std::string& GetTexturePath(LineTexture lineTexture)
{ return lineTextureInfo_.at(lineTexture).path_; }

} // namespace types
} // namespace qt
} // namespace scwx
