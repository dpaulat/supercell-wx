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
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiResourceUpdateBatch;
class QRhiTexture;

namespace scwx::qt::render
{

class RhiColoredGeometry
{
public:
   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer);
   void Shutdown();

   // Call once per overlay frame before the Upload pass. Shared buffer
   // accumulates all drawer uploads; projection is baked into vertices so a
   // single identity UBO works for every draw (avoids mid-pass UBO clobber).
   void BeginFrame();

   // After Upload pass recording: write staging verts + identity UBO into
   // batch.
   void UploadFrame(QRhiResourceUpdateBatch* resourceBatch);

   // Backdrop copy of the destination color buffer for in-shader alpha mix.
   // Must be set before Draw (texture != the active color target).
   void SetBackdrop(QRhiTexture* texture, QRhiSampler* sampler);

   void Render(QRhiCommandBuffer*        commandBuffer,
               const glm::mat4&          projection,
               const std::vector<float>& vertices,
               std::size_t               vertexCount,
               QRhiResourceUpdateBatch*  resourceBatch = nullptr,
               RhiOverlayPhase phase = RhiOverlayPhase::UploadAndDraw);

   [[nodiscard]] bool IsInitialized() const;

private:
   struct DrawCommand
   {
      std::size_t firstVertex {0};
      std::size_t vertexCount {0};
   };

   bool EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);
   bool BindResources();
   void AppendTransformed(const glm::mat4&          projection,
                          const std::vector<float>& vertices,
                          std::size_t               vertexCount);

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 uniformBuffer_ {nullptr};
   QRhiBuffer*                 vertexBuffer_ {nullptr};
   QRhiShaderResourceBindings* srb_ {nullptr};
   QRhiGraphicsPipeline*       pipeline_ {nullptr};
   QRhiTexture*                backdropTexture_ {nullptr};
   QRhiSampler*                backdropSampler_ {nullptr};
   std::size_t                 vertexCapacity_ {0};
   bool                        initialized_ {false};

   std::vector<float>       stagingVertices_ {};
   std::vector<DrawCommand> drawCommands_ {};
   std::size_t              drawCursor_ {0};
   bool                     frameUploaded_ {false};
};

} // namespace scwx::qt::render
