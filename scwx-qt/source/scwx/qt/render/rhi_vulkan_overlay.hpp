#pragma once

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
};

} // namespace scwx::qt::render
