#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_radar_overlay.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_radar_overlay";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kUniformBytes = sizeof(RadarUniforms);
static constexpr int kMaxLutWidth  = 512;

static void ExpandMoments(const std::vector<std::uint8_t>& source,
                          std::size_t                      componentSize,
                          std::size_t                      vertexCount,
                          std::vector<std::uint32_t>&      output)
{
   output.assign(vertexCount, 0u);

   if (componentSize == 1)
   {
      const std::size_t count = std::min(vertexCount, source.size());
      for (std::size_t i = 0; i < count; ++i)
      {
         output[i] = source[i];
      }
   }
   else if (componentSize == 2)
   {
      const std::size_t count =
         std::min(vertexCount, source.size() / sizeof(std::uint16_t));
      for (std::size_t i = 0; i < count; ++i)
      {
         std::uint16_t value {};
         std::memcpy(&value, source.data() + i * sizeof(value), sizeof(value));
         output[i] = value;
      }
   }
}

void RhiRadarOverlay::Initialize(QRhi*             rhi,
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

   vertexBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (vertexBuffer_ == nullptr || !vertexBuffer_->create())
   {
      Shutdown();
      return;
   }

   momentBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (momentBuffer_ == nullptr || !momentBuffer_->create())
   {
      Shutdown();
      return;
   }

   cfpBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (cfpBuffer_ == nullptr || !cfpBuffer_->create())
   {
      Shutdown();
      return;
   }

   sampler_ = AcquireNearestSampler(rhi_);
   if (sampler_ == nullptr)
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

bool RhiRadarOverlay::EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   pipeline_ = AcquireRadarPipeline(rhi, renderTarget);
   if (pipeline_ == nullptr)
   {
      return false;
   }

   if (lutTexture_ == nullptr)
   {
      lutTexture_ = rhi->newTexture(QRhiTexture::RGBA8, QSize(kMaxLutWidth, 1));
      if (lutTexture_ == nullptr || !lutTexture_->create())
      {
         logger_->error("Failed to create radar LUT texture");
         return false;
      }
   }

   if (srb_ == nullptr)
   {
      srb_ = rhi->newShaderResourceBindings();
      if (srb_ == nullptr)
      {
         logger_->error("Failed to allocate radar shader bindings");
         return false;
      }
   }
   srb_->setBindings({
      QRhiShaderResourceBinding::uniformBuffer(
         0,
         QRhiShaderResourceBinding::VertexStage |
            QRhiShaderResourceBinding::FragmentStage,
         uniformBuffer_),
      QRhiShaderResourceBinding::sampledTexture(
         1, QRhiShaderResourceBinding::FragmentStage, lutTexture_, sampler_),
   });
   if (!srb_->create())
   {
      return false;
   }

   return true;
}

void RhiRadarOverlay::Shutdown()
{
   // Pipeline and nearest sampler owned by shared GPU store.
   pipeline_ = nullptr;
   sampler_  = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete lutTexture_;
   lutTexture_ = nullptr;
   delete cfpBuffer_;
   cfpBuffer_ = nullptr;
   delete momentBuffer_;
   momentBuffer_ = nullptr;
   delete vertexBuffer_;
   vertexBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_  = nullptr;
   rhi_            = nullptr;
   renderTarget_   = nullptr;
   vertexCapacity_ = 0;
   momentCapacity_ = 0;
   cfpCapacity_    = 0;
   momentU32_.clear();
   cfpU32_.clear();
   lutRgba_.clear();
   geometryUploaded_             = false;
   lutUploaded_                  = false;
   uploadedVertexCount_          = 0;
   uploadedGeometryGeneration_   = 0;
   uploadedColorTableGeneration_ = 0;
   initialized_                  = false;
}

void RhiRadarOverlay::Render(QRhiCommandBuffer*               commandBuffer,
                             const RadarUniforms&             uniforms,
                             const std::vector<float>&        vertices,
                             const std::vector<std::uint8_t>& momentData,
                             const std::size_t momentComponentSize,
                             const std::vector<std::uint8_t>& cfpData,
                             const std::size_t                cfpComponentSize,
                             const std::vector<std::uint8_t>& rgbaColorTable,
                             const std::uint32_t              vertexCount,
                             bool                             uploadGeometry,
                             bool                             uploadColorTable,
                             const std::uint64_t      geometryGeneration,
                             const std::uint64_t      colorTableGeneration,
                             QRhiResourceUpdateBatch* resourceBatch,
                             RhiOverlayPhase          phase)
{
   if (!initialized_ || pipeline_ == nullptr || commandBuffer == nullptr ||
       vertexCount == 0 || rgbaColorTable.empty() ||
       uniforms.uDataMomentScale <= 0.0f)
   {
      return;
   }

   const int tableWidth = static_cast<int>(rgbaColorTable.size() / 4);
   if (tableWidth <= 0 || vertices.size() < vertexCount * 2u)
   {
      return;
   }

   const std::size_t vertexBytes = vertexCount * 2u * sizeof(float);
   const std::size_t momentBytes = vertexCount * sizeof(std::uint32_t);
   uploadGeometry                = uploadGeometry || !geometryUploaded_ ||
                    vertexCount != uploadedVertexCount_ ||
                    vertexCapacity_ < vertexBytes ||
                    momentCapacity_ < momentBytes || cfpCapacity_ < momentBytes;
   uploadColorTable = uploadColorTable || !lutUploaded_;

   // The per-pane GPU buffers and LUT outlive the layer (and view) that filled
   // them. Never rely on the caller's "upload needed" flags alone: also upload
   // whenever the GPU copy was produced from a different data generation, so a
   // lost flag cannot leave the previous product's moments or colors on the
   // GPU.
   uploadGeometry =
      uploadGeometry || geometryGeneration != uploadedGeometryGeneration_;
   uploadColorTable =
      uploadColorTable || colorTableGeneration != uploadedColorTableGeneration_;

   if (!OverlayShouldUpload(phase) && (uploadGeometry || uploadColorTable))
   {
      // The draw-only phase cannot upload. Drawing now would show whatever
      // product was previously resident on the GPU, so skip this frame; the
      // next upload phase brings the GPU copy up to date.
      return;
   }

   if (uploadGeometry)
   {
      ExpandMoments(momentData, momentComponentSize, vertexCount, momentU32_);
      ExpandMoments(cfpData, cfpComponentSize, vertexCount, cfpU32_);

      if (!EnsureDynamicBuffer(rhi_,
                               vertexBuffer_,
                               vertexCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               vertexBytes) ||
          !EnsureDynamicBuffer(rhi_,
                               momentBuffer_,
                               momentCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               momentBytes) ||
          !EnsureDynamicBuffer(rhi_,
                               cfpBuffer_,
                               cfpCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               momentBytes))
      {
         return;
      }
   }

   if (uploadColorTable)
   {
      lutRgba_.resize(kMaxLutWidth * 4);
      for (int i = 0; i < kMaxLutWidth; ++i)
      {
         // Texel i covers [i, i + 1) / kMaxLutWidth of the table. Sample at
         // its center so each source entry maps to the texels it spans.
         const int sourceIndex = std::min(
            tableWidth - 1,
            static_cast<int>(((2 * static_cast<std::int64_t>(i) + 1) *
                              static_cast<std::int64_t>(tableWidth)) /
                             static_cast<std::int64_t>(2 * kMaxLutWidth)));
         std::copy_n(rgbaColorTable.data() + sourceIndex * 4,
                     4,
                     lutRgba_.data() + i * 4);
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
         batch->updateDynamicBuffer(vertexBuffer_,
                                    0,
                                    static_cast<quint32>(vertexBytes),
                                    vertices.data());
         batch->updateDynamicBuffer(momentBuffer_,
                                    0,
                                    static_cast<quint32>(momentBytes),
                                    momentU32_.data());
         batch->updateDynamicBuffer(
            cfpBuffer_, 0, static_cast<quint32>(momentBytes), cfpU32_.data());
      }

      if (uploadColorTable)
      {
         const QRhiTextureSubresourceUploadDescription subUpload(
            lutRgba_.data(), static_cast<quint32>(kMaxLutWidth * 4));
         batch->uploadTexture(lutTexture_,
                              QRhiTextureUploadDescription(
                                 QRhiTextureUploadEntry(0, 0, subUpload)));
      }

      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
      if (uploadGeometry)
      {
         uploadedVertexCount_        = vertexCount;
         uploadedGeometryGeneration_ = geometryGeneration;

         // The update batch owns copies of the expanded moments, which are
         // rebuilt from the source data for every geometry upload. Release the
         // scratch so it does not stay resident between sweeps.
         std::vector<std::uint32_t>().swap(momentU32_);
         std::vector<std::uint32_t>().swap(cfpU32_);
      }
      geometryUploaded_ = geometryUploaded_ || uploadGeometry;
      lutUploaded_      = lutUploaded_ || uploadColorTable;
      if (uploadColorTable)
      {
         uploadedColorTableGeneration_ = colorTableGeneration;
      }
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   const QRhiCommandBuffer::VertexInput bindings[] = {
      {vertexBuffer_, 0}, {momentBuffer_, 0}, {cfpBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(pipeline_);
   commandBuffer->setShaderResources(srb_);
   commandBuffer->setVertexInput(0, 3, bindings);
   commandBuffer->draw(vertexCount);
}

bool RhiRadarOverlay::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::render
