#pragma once

#include <scwx/qt/render/rhi_geo_uniforms.hpp>

#include <glm/glm.hpp>

#include <cstddef>
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

namespace scwx::qt::render
{

class RhiTextureArrayOverlay
{
public:
   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer);
   void Shutdown();

   void SyncAtlas(QRhiCommandBuffer* commandBuffer, std::uint64_t buildCount);

   void RenderGeo(QRhiCommandBuffer*               commandBuffer,
                  const GeoUniforms&               uniforms,
                  const std::vector<float>&        floatVertices,
                  const std::vector<std::int32_t>& integerVertices,
                  std::uint32_t                    vertexCount);

   void RenderScreen(QRhiCommandBuffer*        commandBuffer,
                     const glm::mat4&          projection,
                     const std::vector<float>& floatVertices,
                     const std::vector<float>& texCoords,
                     std::uint32_t             vertexCount);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool EnsureGeoPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);
   bool EnsureScreenPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);
   bool EnsureShaderResources(QRhi* rhi);

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 geoUniformBuffer_ {nullptr};
   QRhiBuffer*                 geoFloatBuffer_ {nullptr};
   QRhiBuffer*                 geoIntegerBuffer_ {nullptr};
   QRhiBuffer*                 screenUniformBuffer_ {nullptr};
   QRhiBuffer*                 screenFloatBuffer_ {nullptr};
   QRhiBuffer*                 screenTexCoordBuffer_ {nullptr};
   QRhiTexture*                atlasTexture_ {nullptr};
   QRhiSampler*                atlasSampler_ {nullptr};
   QRhiShaderResourceBindings* geoSrb_ {nullptr};
   QRhiShaderResourceBindings* screenSrb_ {nullptr};
   QRhiGraphicsPipeline*       geoPipeline_ {nullptr};
   QRhiGraphicsPipeline*       screenPipeline_ {nullptr};
   std::size_t                 geoFloatCapacity_ {0};
   std::size_t                 geoIntegerCapacity_ {0};
   std::size_t                 screenFloatCapacity_ {0};
   std::size_t                 screenTexCoordCapacity_ {0};
   std::size_t                 atlasWidth_ {0};
   std::size_t                 atlasHeight_ {0};
   std::size_t                 atlasLayers_ {0};
   std::uint64_t               syncedBuildCount_ {0};
   bool                        initialized_ {false};
};

} // namespace scwx::qt::render
