#include <scwx/qt/render/rhi_color_table_overlay.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ =
   "scwx::qt::render::rhi_color_table_overlay";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

struct ColorTableUniforms
{
   alignas(16) glm::mat4 uMVPMatrix {};
   alignas(4) float uOpacity {1.0f};
   alignas(4) float _pad0 {0.0f};
   alignas(8) glm::vec2 _pad1 {};
};

static_assert(sizeof(ColorTableUniforms) == 80);
static constexpr int kUniformBytes =
   static_cast<int>(sizeof(ColorTableUniforms));
static constexpr int kMaxLutWidth = 512;

void RhiColorTableOverlay::Initialize(QRhi*              rhi,
                                      QRhiRenderTarget*  renderTarget,
                                      QRhiCommandBuffer* commandBuffer)
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

   vertexBuffer_ = rhi_->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(float) * 6 * 2);
   if (vertexBuffer_ == nullptr || !vertexBuffer_->create())
   {
      Shutdown();
      return;
   }

   static constexpr float kTexCoords[6] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
   texCoordBuffer_                      = rhi_->newBuffer(
      QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kTexCoords));
   if (texCoordBuffer_ == nullptr || !texCoordBuffer_->create())
   {
      Shutdown();
      return;
   }

   if (commandBuffer != nullptr)
   {
      if (QRhiResourceUpdateBatch* batch = rhi_->nextResourceUpdateBatch())
      {
         batch->uploadStaticBuffer(texCoordBuffer_, kTexCoords);
         commandBuffer->resourceUpdate(batch);
      }
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

bool RhiColorTableOverlay::EnsurePipeline(QRhi*             rhi,
                                          QRhiRenderTarget* renderTarget)
{
   pipeline_ = AcquireColorTablePipeline(rhi, renderTarget);
   if (pipeline_ == nullptr)
   {
      return false;
   }

   if (lutTexture_ == nullptr)
   {
      lutTexture_ = rhi->newTexture(QRhiTexture::RGBA8, QSize(kMaxLutWidth, 1));
      if (lutTexture_ == nullptr || !lutTexture_->create())
      {
         logger_->error("Failed to create color table texture");
         return false;
      }
      lutWidth_ = kMaxLutWidth;
   }

   if (srb_ == nullptr)
   {
      srb_ = rhi->newShaderResourceBindings();
      if (srb_ == nullptr)
      {
         logger_->error("Failed to allocate shader resource bindings");
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
      logger_->error("Failed to create shader resource bindings");
      return false;
   }

   return true;
}

void RhiColorTableOverlay::Shutdown()
{
   // Pipeline and nearest sampler owned by shared GPU store.
   pipeline_ = nullptr;
   sampler_  = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete lutTexture_;
   lutTexture_ = nullptr;
   delete texCoordBuffer_;
   texCoordBuffer_ = nullptr;
   delete vertexBuffer_;
   vertexBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_ = nullptr;
   rhi_           = nullptr;
   renderTarget_  = nullptr;
   lutWidth_      = 0;
   lutUploaded_   = false;
   uploadedLut_.clear();
   initialized_ = false;
}

void RhiColorTableOverlay::Render(
   QRhiCommandBuffer*               commandBuffer,
   const glm::mat4&                 projection,
   const float                      vertices[6][2],
   const std::vector<std::uint8_t>& rgbaColorTable,
   QRhiResourceUpdateBatch*         resourceBatch,
   RhiOverlayPhase                  phase,
   const float                      opacity)
{
   if (!initialized_ || commandBuffer == nullptr || rgbaColorTable.empty())
   {
      return;
   }

   const int tableWidth = static_cast<int>(rgbaColorTable.size() / 4);
   if (tableWidth <= 0)
   {
      return;
   }

   if (OverlayShouldUpload(phase))
   {
      QRhiResourceUpdateBatch* batch =
         AcquireOverlayBatch(rhi_, resourceBatch, phase);
      if (batch == nullptr)
      {
         return;
      }
      ColorTableUniforms uniforms {};
      uniforms.uMVPMatrix = projection;
      uniforms.uOpacity   = opacity;
      batch->updateDynamicBuffer(uniformBuffer_, 0, kUniformBytes, &uniforms);
      batch->updateDynamicBuffer(
         vertexBuffer_, 0, sizeof(float) * 6 * 2, vertices);

      const bool lutChanged = !lutUploaded_ ||
                              uploadedLut_.size() != rgbaColorTable.size() ||
                              std::memcmp(uploadedLut_.data(),
                                          rgbaColorTable.data(),
                                          rgbaColorTable.size()) != 0;
      if (lutChanged)
      {
         // Texture coordinates span the full texture, so resample the table
         // to the texture width (unwritten texels would otherwise be garbage).
         lutRgba_.resize(static_cast<std::size_t>(kMaxLutWidth) * 4);
         for (int i = 0; i < kMaxLutWidth; ++i)
         {
            const int sourceIndex = std::min(
               tableWidth - 1,
               static_cast<int>(((2 * static_cast<std::int64_t>(i) + 1) *
                                 static_cast<std::int64_t>(tableWidth)) /
                                static_cast<std::int64_t>(2 * kMaxLutWidth)));
            std::copy_n(rgbaColorTable.data() + sourceIndex * 4,
                        4,
                        lutRgba_.data() + i * 4);
         }
         const QRhiTextureSubresourceUploadDescription subUpload(
            lutRgba_.data(), static_cast<quint32>(kMaxLutWidth * 4));
         const QRhiTextureUploadDescription upload(
            QRhiTextureUploadEntry(0, 0, subUpload));
         batch->uploadTexture(lutTexture_, upload);
         uploadedLut_ = rgbaColorTable;
         lutUploaded_ = true;
      }

      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   const QRhiCommandBuffer::VertexInput bindings[] = {{vertexBuffer_, 0},
                                                      {texCoordBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(pipeline_);
   commandBuffer->setShaderResources(srb_);
   commandBuffer->setVertexInput(0, 2, bindings);
   commandBuffer->draw(6);
}

bool RhiColorTableOverlay::IsInitialized() const
{ return initialized_; }

} // namespace scwx::qt::render
