#pragma once

#include <scwx/qt/render/rhi_overlay_phase.hpp>

#include <glm/glm.hpp>

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

class RhiColoredGeometry
{
public:
   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer);
   void Shutdown();

   void Render(QRhiCommandBuffer*        commandBuffer,
               const glm::mat4&          projection,
               const std::vector<float>& vertices,
               std::size_t               vertexCount,
               QRhiResourceUpdateBatch*  resourceBatch = nullptr,
               RhiOverlayPhase           phase         = RhiOverlayPhase::UploadAndDraw);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 uniformBuffer_ {nullptr};
   QRhiBuffer*                 vertexBuffer_ {nullptr};
   QRhiShaderResourceBindings* srb_ {nullptr};
   QRhiGraphicsPipeline*       pipeline_ {nullptr};
   std::size_t                 vertexCapacity_ {0};
   bool                        initialized_ {false};
};

} // namespace scwx::qt::render
