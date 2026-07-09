#pragma once

#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/rhi_overlay_phase.hpp>

#include <cstddef>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderTarget;
class QRhiShaderResourceBindings;
class QRhiResourceUpdateBatch;

namespace scwx::qt::render
{

class RhiGeoColoredGeometry
{
public:
   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer);
   void Shutdown();

   void Render(QRhiCommandBuffer*               commandBuffer,
               const GeoUniforms&               uniforms,
               const std::vector<float>&        floatVertices,
               const std::vector<std::int32_t>& integerVertices,
               std::uint32_t                    vertexCount,
               bool                             uploadGeometry = true,
               QRhiResourceUpdateBatch*         resourceBatch  = nullptr,
               RhiOverlayPhase                  phase          = RhiOverlayPhase::UploadAndDraw);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 uniformBuffer_ {nullptr};
   QRhiBuffer*                 floatBuffer_ {nullptr};
   QRhiBuffer*                 integerBuffer_ {nullptr};
   QRhiShaderResourceBindings* srb_ {nullptr};
   QRhiGraphicsPipeline*       pipeline_ {nullptr};
   std::size_t                 floatCapacity_ {0};
   std::size_t                 integerCapacity_ {0};
   bool                        initialized_ {false};
};

} // namespace scwx::qt::render
