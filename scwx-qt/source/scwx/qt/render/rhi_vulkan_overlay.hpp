#pragma once

#include <cstdint>

class QRhi;
class QRhiRenderTarget;

namespace scwx::qt::render
{

class RhiColorTableOverlay;
class RhiColoredGeometry;
class RhiGeoColoredGeometry;
class RhiRadarOverlay;
class RhiTextureArrayOverlay;

struct RhiVulkanOverlayResources
{
   RhiColorTableOverlay&   colorTable;
   RhiRadarOverlay&        radar;
   RhiColoredGeometry&     coloredGeometry;
   RhiGeoColoredGeometry&  radarGeoColoredGeometry;
   RhiGeoColoredGeometry&  geoColoredGeometry;
   RhiTextureArrayOverlay& textureArrayOverlay;
   QRhi*                   rhi {nullptr};
   QRhiRenderTarget*       renderTarget {nullptr};
   std::uint64_t           renderTargetGeneration {0};
};

} // namespace scwx::qt::render
