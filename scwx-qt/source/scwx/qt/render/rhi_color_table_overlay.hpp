#pragma once

#include <scwx/qt/render/rhi_overlay_phase.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderTarget;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
class QRhiResourceUpdateBatch;

namespace scwx::qt::render
{

class RhiColorTableOverlay
{
public:
   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer);
   void Shutdown();

   void Render(QRhiCommandBuffer*               commandBuffer,
               const glm::mat4&                 projection,
               const float                      vertices[6][2],
               const std::vector<std::uint8_t>& rgbaColorTable,
               QRhiResourceUpdateBatch*         resourceBatch = nullptr,
               RhiOverlayPhase phase = RhiOverlayPhase::UploadAndDraw);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 uniformBuffer_ {nullptr};
   QRhiBuffer*                 vertexBuffer_ {nullptr};
   QRhiBuffer*                 texCoordBuffer_ {nullptr};
   QRhiTexture*                lutTexture_ {nullptr};
   QRhiSampler*                sampler_ {nullptr};
   QRhiShaderResourceBindings* srb_ {nullptr};
   QRhiGraphicsPipeline*       pipeline_ {nullptr};
   int                         lutWidth_ {0};
   std::vector<std::uint8_t>   uploadedLut_ {};
   bool                        lutUploaded_ {false};
   bool                        initialized_ {false};
};

} // namespace scwx::qt::render
