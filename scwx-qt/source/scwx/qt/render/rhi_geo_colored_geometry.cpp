#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/util/logger.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ =
   "scwx::qt::render::rhi_geo_colored_geometry";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr int kUniformBytes = 144;

void RhiGeoColoredGeometry::Initialize(QRhi*             rhi,
                                       QRhiRenderTarget* renderTarget,
                                       QRhiCommandBuffer* /* commandBuffer */)
{
   if (initialized_ || rhi == nullptr || renderTarget == nullptr)
   {
      return;
   }

   rhi_          = rhi;
   renderTarget_ = renderTarget;

   uniformBuffer_ = rhi_->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes);
   if (uniformBuffer_ == nullptr || !uniformBuffer_->create())
   {
      Shutdown();
      return;
   }

   floatBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (floatBuffer_ == nullptr || !floatBuffer_->create())
   {
      Shutdown();
      return;
   }

   integerBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (integerBuffer_ == nullptr || !integerBuffer_->create())
   {
      Shutdown();
      return;
   }

   if (!EnsurePipeline(rhi_, renderTarget_))
   {
      Shutdown();
      return;
   }

   initialized_ = true;
}

bool RhiGeoColoredGeometry::EnsurePipeline(QRhi*             rhi,
                                           QRhiRenderTarget* renderTarget)
{
   pipeline_ = AcquireGeoColoredGeometryPipeline(rhi, renderTarget);
   if (pipeline_ == nullptr)
   {
      return false;
   }

   if (srb_ == nullptr)
   {
      srb_ = rhi->newShaderResourceBindings();
      if (srb_ == nullptr)
      {
         logger_->error("Failed to allocate geo colored shader bindings");
         return false;
      }
   }
   srb_->setBindings({QRhiShaderResourceBinding::uniformBuffer(
      0,
      QRhiShaderResourceBinding::VertexStage |
         QRhiShaderResourceBinding::FragmentStage,
      uniformBuffer_)});
   if (!srb_->create())
   {
      return false;
   }

   return true;
}

void RhiGeoColoredGeometry::Shutdown()
{
   // Pipeline owned by shared GPU store.
   pipeline_ = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete integerBuffer_;
   integerBuffer_ = nullptr;
   delete floatBuffer_;
   floatBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_   = nullptr;
   rhi_             = nullptr;
   renderTarget_    = nullptr;
   floatCapacity_   = 0;
   integerCapacity_ = 0;
   initialized_     = false;
}

void RhiGeoColoredGeometry::Render(
   QRhiCommandBuffer*               commandBuffer,
   const GeoUniforms&               uniforms,
   const std::vector<float>&        floatVertices,
   const std::vector<std::int32_t>& integerVertices,
   const std::uint32_t              vertexCount,
   bool                             uploadGeometry,
   QRhiResourceUpdateBatch*         resourceBatch,
   RhiOverlayPhase                  phase)
{
   if (!initialized_ || commandBuffer == nullptr || vertexCount == 0)
   {
      return;
   }

   const std::size_t floatBytes = floatVertices.size() * sizeof(float);
   const std::size_t integerBytes =
      integerVertices.size() * sizeof(std::int32_t);
   uploadGeometry = uploadGeometry || floatCapacity_ < floatBytes ||
                    integerCapacity_ < integerBytes;

   if (OverlayShouldUpload(phase) && uploadGeometry &&
       floatCapacity_ < floatBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               floatBuffer_,
                               floatCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               floatBytes))
      {
         return;
      }
   }
   if (OverlayShouldUpload(phase) && uploadGeometry &&
       integerCapacity_ < integerBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               integerBuffer_,
                               integerCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               integerBytes))
      {
         return;
      }
   }

   if (OverlayShouldUpload(phase))
   {
      QRhiResourceUpdateBatch* batch =
         AcquireOverlayBatch(rhi_, resourceBatch, phase);
      if (batch == nullptr)
      {
         return;
      }
      batch->updateDynamicBuffer(uniformBuffer_, 0, kUniformBytes, &uniforms);
      if (uploadGeometry)
      {
         batch->updateDynamicBuffer(floatBuffer_,
                                    0,
                                    static_cast<quint32>(floatBytes),
                                    floatVertices.data());
         batch->updateDynamicBuffer(integerBuffer_,
                                    0,
                                    static_cast<quint32>(integerBytes),
                                    integerVertices.data());
      }
      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   const QRhiCommandBuffer::VertexInput bindings[] = {{floatBuffer_, 0},
                                                      {integerBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(pipeline_);
   commandBuffer->setShaderResources(srb_);
   commandBuffer->setVertexInput(0, 2, bindings);
   commandBuffer->draw(vertexCount);
}

bool RhiGeoColoredGeometry::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::render
