#pragma once

#include <cstddef>
#include <cstdint>

class QRhi;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderTarget;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiTexture;

namespace scwx::qt::render
{

enum class RhiOverlayPhase;

struct RhiSharedAtlas
{
   QRhiTexture*  texture_ {nullptr};
   QRhiSampler*  sampler_ {nullptr};
   std::size_t   width_ {0};
   std::size_t   height_ {0};
   std::size_t   layers_ {0};
   std::uint64_t buildCount_ {0};
};

[[nodiscard]] RhiSharedAtlas
AcquireSharedAtlas(QRhi*                    rhi,
                   QRhiCommandBuffer*       commandBuffer,
                   std::uint64_t            buildCount,
                   QRhiResourceUpdateBatch* resourceBatch,
                   RhiOverlayPhase          phase);

[[nodiscard]] QRhiGraphicsPipeline*
AcquireColoredGeometryPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

[[nodiscard]] QRhiGraphicsPipeline*
AcquireGeoColoredGeometryPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

[[nodiscard]] QRhiGraphicsPipeline*
AcquireRadarPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

[[nodiscard]] QRhiGraphicsPipeline*
AcquireColorTablePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

[[nodiscard]] QRhiGraphicsPipeline*
AcquireTextureArrayGeoPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

[[nodiscard]] QRhiGraphicsPipeline*
AcquireTextureArrayScreenPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

[[nodiscard]] QRhiSampler* AcquireNearestSampler(QRhi* rhi);

// Retain/release shared GPU resources for a QRhi. Multiple MapWidgets in the
// same top-level window share one QRhi and must pair retain/release.
void RetainOverlayGpuStore(QRhi* rhi);
void ReleaseOverlayGpuStore(QRhi* rhi);

} // namespace scwx::qt::render
