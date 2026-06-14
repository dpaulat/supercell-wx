#pragma once

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

namespace scwx::qt::render
{

struct RadarUniforms
{
   alignas(16) glm::mat4 uMVPMatrix {};
   alignas(16) glm::mat4 uMapMatrix {};
   alignas(8) glm::vec2 uOriginLatLong {};
   alignas(4) std::uint32_t uDataMomentOffset {0};
   alignas(4) float uDataMomentScale {1.0f};
   alignas(4) std::int32_t uCFPEnabled {0};
   alignas(4) std::uint32_t _pad {0};
};

class RhiRadarOverlay
{
public:
   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer);
   void Shutdown();

   void Render(QRhiCommandBuffer*               commandBuffer,
               const RadarUniforms&             uniforms,
               const std::vector<float>&        vertices,
               const std::vector<std::uint8_t>& momentData,
               std::size_t                      momentComponentSize,
               const std::vector<std::uint8_t>& cfpData,
               std::size_t                      cfpComponentSize,
               const std::vector<std::uint8_t>& rgbaColorTable,
               std::uint32_t                    vertexCount,
               bool                             uploadGeometry,
               bool                             uploadColorTable);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 uniformBuffer_ {nullptr};
   QRhiBuffer*                 vertexBuffer_ {nullptr};
   QRhiBuffer*                 momentBuffer_ {nullptr};
   QRhiBuffer*                 cfpBuffer_ {nullptr};
   QRhiTexture*                lutTexture_ {nullptr};
   QRhiSampler*                sampler_ {nullptr};
   QRhiShaderResourceBindings* srb_ {nullptr};
   QRhiGraphicsPipeline*       pipeline_ {nullptr};
   std::size_t                 vertexCapacity_ {0};
   std::size_t                 momentCapacity_ {0};
   std::size_t                 cfpCapacity_ {0};
   std::vector<std::uint32_t>  momentU32_ {};
   std::vector<std::uint32_t>  cfpU32_ {};
   std::vector<std::uint8_t>   lutRgba_ {};
   bool                        geometryUploaded_ {false};
   bool                        lutUploaded_ {false};
   bool                        initialized_ {false};
};

} // namespace scwx::qt::render
