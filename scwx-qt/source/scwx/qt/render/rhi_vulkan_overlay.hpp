#pragma once

#include <scwx/qt/render/rhi_overlay_phase.hpp>

#include <cstdint>

class QRhi;
class QRhiCommandBuffer;
class QRhiRenderTarget;
class QRhiResourceUpdateBatch;

namespace scwx::qt::render
{

class RhiColorTableOverlay;
class RhiColoredGeometry;
class RhiGeoColoredGeometry;
class RhiRadarOverlay;
class RhiTextureArrayOverlay;

struct RhiVulkanOverlayResources
{
   RhiColorTableOverlay&    colorTable;
   RhiRadarOverlay&         radar;
   RhiColoredGeometry&      coloredGeometry;
   RhiGeoColoredGeometry&   radarGeoColoredGeometry;
   RhiGeoColoredGeometry&   geoColoredGeometry;
   RhiTextureArrayOverlay&  textureArrayOverlay;
   QRhi*                    rhi {nullptr};
   QRhiRenderTarget*        renderTarget {nullptr};
   std::uint64_t            renderTargetGeneration {0};
   QRhiResourceUpdateBatch* resourceBatch {nullptr};
   RhiOverlayPhase          phase {RhiOverlayPhase::UploadAndDraw};
};

} // namespace scwx::qt::render
