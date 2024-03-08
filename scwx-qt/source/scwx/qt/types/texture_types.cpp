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
   {ImageTexture::MapboxLogo,
    {"images/mapbox-logo", ":/res/textures/images/mapbox-logo.svg"}},
   {ImageTexture::MapTilerLogo,
    {"images/maptiler-logo", ":/res/textures/images/maptiler-logo.svg"}}};

static const std::unordered_map<LineTexture, TextureInfo> lineTextureInfo_ {
   {LineTexture::Default1x7,
    {"lines/default-1x7", ":/res/textures/lines/default-1x7.png"}},
   {LineTexture::TestPattern,
    {"lines/test-pattern", ":/res/textures/lines/test-pattern.png"}}};

const std::string& GetTextureName(ImageTexture imageTexture)
{
   return imageTextureInfo_.at(imageTexture).name_;
}

const std::string& GetTextureName(LineTexture lineTexture)
{
   return lineTextureInfo_.at(lineTexture).name_;
}

const std::string& GetTexturePath(ImageTexture imageTexture)
{
   return imageTextureInfo_.at(imageTexture).path_;
}

const std::string& GetTexturePath(LineTexture lineTexture)
{
   return lineTextureInfo_.at(lineTexture).path_;
}

} // namespace types
} // namespace qt
} // namespace scwx
