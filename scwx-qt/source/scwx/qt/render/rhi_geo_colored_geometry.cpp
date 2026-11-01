#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/util/logger.hpp>

#include <rhi/qrhi.h>

#include <algorithm>

namespace scwx::qt::render
{

static const std::string logPrefix_ =
   "scwx::qt::render::rhi_geo_colored_geometry";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr int kUniformBytes = static_cast<int>(sizeof(GeoUniforms));

// Maximum geometry bytes handed to QRhi per overlay frame, so enabling a very
// large placefile does not stall the GUI thread in a single upload.
static constexpr std::size_t kFloatChunkBytes   = 6u * 1024u * 1024u;
static constexpr std::size_t kIntegerChunkBytes = 2u * 1024u * 1024u;

void RhiGeoColoredGeometry::Initialize(QRhi*             rhi,
                                       QRhiRenderTarget* renderTarget,
                                       QRhiCommandBuffer* /* commandBuffer */,
                                       Layout layout)
{
   if (initialized_ || rhi == nullptr || renderTarget == nullptr)
   {
      return;
   }

   rhi_          = rhi;
   renderTarget_ = renderTarget;
   layout_       = layout;

   uniformBuffer_ = rhi_->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes);
   if (uniformBuffer_ == nullptr || !uniformBuffer_->create())
   {
      Shutdown();
      return;
   }

   if (layout_ == Layout::Compact)
   {
      constantBuffer_ = rhi_->newBuffer(
         QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 12 * sizeof(float));
      if (constantBuffer_ == nullptr || !constantBuffer_->create())
      {
         Shutdown();
         return;
      }
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
   pipeline_ = AcquireGeoColoredGeometryPipeline(
      rhi, renderTarget, layout_ == Layout::Compact);
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

void RhiGeoColoredGeometry::ReleasePending()
{
   delete pendingFloatBuffer_;
   pendingFloatBuffer_ = nullptr;
   delete pendingIntegerBuffer_;
   pendingIntegerBuffer_ = nullptr;
   pendingFloatBytes_    = 0;
   pendingIntegerBytes_  = 0;
   pendingFloatOffset_   = 0;
   pendingIntegerOffset_ = 0;
   pendingVertexCount_   = 0;
   uploadActive_         = false;
}

void RhiGeoColoredGeometry::Shutdown()
{
   // Pipeline owned by shared GPU store.
   pipeline_ = nullptr;
   ReleasePending();
   delete srb_;
   srb_ = nullptr;
   delete integerBuffer_;
   integerBuffer_ = nullptr;
   delete floatBuffer_;
   floatBuffer_ = nullptr;
   delete constantBuffer_;
   constantBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_      = nullptr;
   rhi_                = nullptr;
   renderTarget_       = nullptr;
   constantUploaded_   = false;
   activeVertexCount_  = 0;
   initialized_        = false;
}

bool RhiGeoColoredGeometry::StartPendingUpload(const std::size_t   floatBytes,
                                               const std::size_t   integerBytes,
                                               const std::uint32_t vertexCount)
{
   ReleasePending();

   const auto create = [this](std::size_t bytes) -> QRhiBuffer*
   {
      QRhiBuffer* buffer = rhi_->newBuffer(QRhiBuffer::Immutable,
                                           QRhiBuffer::VertexBuffer,
                                           static_cast<quint32>(bytes));
      if (buffer == nullptr || !buffer->create())
      {
         delete buffer;
         return nullptr;
      }
      return buffer;
   };

   pendingFloatBuffer_   = create(floatBytes);
   pendingIntegerBuffer_ = create(integerBytes);
   if (pendingFloatBuffer_ == nullptr || pendingIntegerBuffer_ == nullptr)
   {
      logger_->error("Failed to allocate geo geometry buffers ({} + {} bytes)",
                     floatBytes,
                     integerBytes);
      ReleasePending();
      return false;
   }

   pendingFloatBytes_   = floatBytes;
   pendingIntegerBytes_ = integerBytes;
   pendingVertexCount_  = vertexCount;
   uploadActive_        = true;
   return true;
}

bool RhiGeoColoredGeometry::Render(
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
      return false;
   }

   bool moreWork = false;

   if (OverlayShouldUpload(phase))
   {
      // Start (or restart) when data changed, or when nothing was ever uploaded
      if (uploadGeometry || (!uploadActive_ && activeVertexCount_ == 0))
      {
         if (!StartPendingUpload(floatVertices.size() * sizeof(float),
                                 integerVertices.size() * sizeof(std::int32_t),
                                 vertexCount))
         {
            return false;
         }
      }

      QRhiResourceUpdateBatch* batch =
         AcquireOverlayBatch(rhi_, resourceBatch, phase);
      if (batch == nullptr)
      {
         return false;
      }
      batch->updateDynamicBuffer(uniformBuffer_, 0, kUniformBytes, &uniforms);

      if (constantBuffer_ != nullptr && !constantUploaded_)
      {
         // highlight/border/strokeHalf = 0, displayed = 1
         static const std::int32_t kConstants[12] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
         batch->uploadStaticBuffer(constantBuffer_, kConstants);
         constantUploaded_ = true;
      }

      if (uploadActive_)
      {
         const auto uploadChunk = [&batch](QRhiBuffer* buffer,
                                           std::size_t totalBytes,
                                           std::size_t& offset,
                                           const void*  data,
                                           std::size_t  chunkBytes)
         {
            if (offset >= totalBytes)
            {
               return;
            }
            const std::size_t n = std::min(chunkBytes, totalBytes - offset);
            batch->uploadStaticBuffer(
               buffer,
               static_cast<quint32>(offset),
               static_cast<quint32>(n),
               static_cast<const char*>(data) + offset);
            offset += n;
         };

         uploadChunk(pendingFloatBuffer_,
                     pendingFloatBytes_,
                     pendingFloatOffset_,
                     floatVertices.data(),
                     kFloatChunkBytes);
         uploadChunk(pendingIntegerBuffer_,
                     pendingIntegerBytes_,
                     pendingIntegerOffset_,
                     integerVertices.data(),
                     kIntegerChunkBytes);

         if (pendingFloatOffset_ >= pendingFloatBytes_ &&
             pendingIntegerOffset_ >= pendingIntegerBytes_)
         {
            // Complete: swap in the new geometry (old buffers are released
            // after the in-flight frames finish with them)
            delete floatBuffer_;
            delete integerBuffer_;
            floatBuffer_          = pendingFloatBuffer_;
            integerBuffer_        = pendingIntegerBuffer_;
            pendingFloatBuffer_   = nullptr;
            pendingIntegerBuffer_ = nullptr;
            activeVertexCount_    = pendingVertexCount_;
            ReleasePending();
         }
         else
         {
            moreWork = true;
         }
      }
      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   }

   if (!OverlayShouldDraw(phase) || floatBuffer_ == nullptr ||
       integerBuffer_ == nullptr || activeVertexCount_ == 0)
   {
      return moreWork;
   }

   if (layout_ == Layout::Compact)
   {
      const QRhiCommandBuffer::VertexInput bindings[] = {
         {floatBuffer_, 0}, {integerBuffer_, 0}, {constantBuffer_, 0}};
      commandBuffer->setGraphicsPipeline(pipeline_);
      commandBuffer->setShaderResources(srb_);
      commandBuffer->setVertexInput(0, 3, bindings);
   }
   else
   {
      const QRhiCommandBuffer::VertexInput bindings[] = {{floatBuffer_, 0},
                                                         {integerBuffer_, 0}};
      commandBuffer->setGraphicsPipeline(pipeline_);
      commandBuffer->setShaderResources(srb_);
      commandBuffer->setVertexInput(0, 2, bindings);
   }
   commandBuffer->draw(activeVertexCount_);

   return moreWork;
}

bool RhiGeoColoredGeometry::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::render
