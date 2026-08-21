#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class ImageTexture
{
   CardinalPoint24,
   Compass24,
   Crosshairs24,
   Cursor17,
   Dot3,
   LocationBriefcase,
   LocationBuildingColumns,
   LocationBuilding,
   LocationCaravan,
   LocationCrosshair,
   LocationHouse,
   LocationMarker,
   LocationPin,
   LocationStar,
   LocationTent,
   MapboxLogo,
   MapTilerLogo,
   OpenFreeMapLogo,
   HailIndex,
   Mesocyclone,
   TornadicVortexSignature
};
typedef scwx::util::Iterator<ImageTexture,
                             ImageTexture::CardinalPoint24,
                             ImageTexture::TornadicVortexSignature>
   ImageTextureIterator;

enum class LineTexture
{
   Default1x7,
   TestPattern
};
typedef scwx::util::
   Iterator<LineTexture, LineTexture::Default1x7, LineTexture::TestPattern>
      LineTextureIterator;

const std::string& GetTextureName(ImageTexture imageTexture);
const std::string& GetTextureName(LineTexture lineTexture);
const std::string& GetTexturePath(ImageTexture imageTexture);
const std::string& GetTexturePath(LineTexture lineTexture);

} // namespace types
} // namespace qt
} // namespace scwx
