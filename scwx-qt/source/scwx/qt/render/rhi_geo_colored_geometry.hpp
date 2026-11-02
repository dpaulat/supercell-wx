#pragma once

#include <scwx/qt/render/rhi_geo_packed_vertex.hpp>
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
   enum class Layout
   {
      /// 20 floats + 4 ints per vertex (stroke/highlight/border/displayed)
      Full,
      /// 9 floats + 3 ints per vertex; stroke attributes are constant
      Compact,
      /// One 28-byte GeoPackedVertex per vertex; stroke attributes are constant
      Packed
   };

   void Initialize(QRhi*              rhi,
                   QRhiRenderTarget*  renderTarget,
                   QRhiCommandBuffer* commandBuffer,
                   Layout             layout = Layout::Full);
   void Shutdown();

   /**
    * Records geometry upload and draw. Geometry is uploaded in bounded chunks
    * into device-local buffers; the previously uploaded geometry keeps drawing
    * until the new geometry is complete.
    *
    * @param uploadGeometry true when the vertex data changed (restarts upload)
    * @return true if more upload work remains (caller should request another
    * frame)
    */
   bool Render(QRhiCommandBuffer*               commandBuffer,
               const GeoUniforms&               uniforms,
               const std::vector<float>&        floatVertices,
               const std::vector<std::int32_t>& integerVertices,
               std::uint32_t                    vertexCount,
               bool                             uploadGeometry = true,
               QRhiResourceUpdateBatch*         resourceBatch  = nullptr,
               RhiOverlayPhase phase = RhiOverlayPhase::UploadAndDraw);

   /**
    * Same as Render, for Layout::Packed geometry.
    */
   bool RenderPacked(QRhiCommandBuffer*                  commandBuffer,
                     const GeoUniforms&                  uniforms,
                     const std::vector<GeoPackedVertex>& vertices,
                     bool                                uploadGeometry = true,
                     QRhiResourceUpdateBatch* resourceBatch = nullptr,
                     RhiOverlayPhase phase = RhiOverlayPhase::UploadAndDraw);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool RenderImpl(QRhiCommandBuffer*       commandBuffer,
                   const GeoUniforms&       uniforms,
                   const void*              floatData,
                   std::size_t              floatBytes,
                   const void*              integerData,
                   std::size_t              integerBytes,
                   std::uint32_t            vertexCount,
                   bool                     uploadGeometry,
                   QRhiResourceUpdateBatch* resourceBatch,
                   RhiOverlayPhase          phase);

   bool EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget);

   bool StartPendingUpload(std::size_t   floatBytes,
                           std::size_t   integerBytes,
                           std::uint32_t vertexCount);
   void ReleasePending();

   QRhi*                       rhi_ {nullptr};
   QRhiRenderTarget*           renderTarget_ {nullptr};
   QRhiBuffer*                 uniformBuffer_ {nullptr};
   QRhiBuffer*                 constantBuffer_ {nullptr};
   QRhiBuffer*                 floatBuffer_ {nullptr};
   QRhiBuffer*                 integerBuffer_ {nullptr};
   QRhiShaderResourceBindings* srb_ {nullptr};
   QRhiGraphicsPipeline*       pipeline_ {nullptr};
   Layout                      layout_ {Layout::Full};
   bool                        constantUploaded_ {false};
   bool                        initialized_ {false};

   // Geometry currently drawn
   std::uint32_t activeVertexCount_ {0};

   // Geometry being uploaded
   QRhiBuffer*   pendingFloatBuffer_ {nullptr};
   QRhiBuffer*   pendingIntegerBuffer_ {nullptr};
   std::size_t   pendingFloatBytes_ {0};
   std::size_t   pendingIntegerBytes_ {0};
   std::size_t   pendingFloatOffset_ {0};
   std::size_t   pendingIntegerOffset_ {0};
   std::uint32_t pendingVertexCount_ {0};
   bool          uploadActive_ {false};
};

} // namespace scwx::qt::render
