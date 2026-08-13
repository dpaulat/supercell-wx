#include <scwx/qt/render/rhi_texture_array_overlay.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/util/logger.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ =
   "scwx::qt::render::rhi_texture_array_overlay";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr int kGeoUniformBytes    = 144;
static constexpr int kScreenUniformBytes = 64;

void RhiTextureArrayOverlay::Initialize(QRhi*             rhi,
                                        QRhiRenderTarget* renderTarget,
                                        QRhiCommandBuffer* /* commandBuffer */)
{
   if (initialized_ || rhi == nullptr || renderTarget == nullptr)
   {
      return;
   }

   rhi_          = rhi;
   renderTarget_ = renderTarget;

   geoUniformBuffer_ = rhi_->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kGeoUniformBytes);
   if (geoUniformBuffer_ == nullptr || !geoUniformBuffer_->create())
   {
      Shutdown();
      return;
   }

   geoFloatBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (geoFloatBuffer_ == nullptr || !geoFloatBuffer_->create())
   {
      Shutdown();
      return;
   }

   geoIntegerBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (geoIntegerBuffer_ == nullptr || !geoIntegerBuffer_->create())
   {
      Shutdown();
      return;
   }

   screenUniformBuffer_ = rhi_->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kScreenUniformBytes);
   if (screenUniformBuffer_ == nullptr || !screenUniformBuffer_->create())
   {
      Shutdown();
      return;
   }

   screenFloatBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (screenFloatBuffer_ == nullptr || !screenFloatBuffer_->create())
   {
      Shutdown();
      return;
   }

   screenTexCoordBuffer_ =
      rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
   if (screenTexCoordBuffer_ == nullptr || !screenTexCoordBuffer_->create())
   {
      Shutdown();
      return;
   }

   initialized_ = true;
}

bool RhiTextureArrayOverlay::EnsureShaderResources(QRhi* /* rhi */)
{
   if (atlasTexture_ == nullptr || atlasSampler_ == nullptr)
   {
      return false;
   }

   if (geoSrb_ == nullptr)
   {
      geoSrb_ = rhi_->newShaderResourceBindings();
      if (geoSrb_ == nullptr)
      {
         logger_->error("Failed to allocate geo texture array shader bindings");
         return false;
      }
   }

   geoSrb_->setBindings({
      QRhiShaderResourceBinding::uniformBuffer(
         0,
         QRhiShaderResourceBinding::VertexStage |
            QRhiShaderResourceBinding::FragmentStage,
         geoUniformBuffer_),
      QRhiShaderResourceBinding::sampledTexture(
         1,
         QRhiShaderResourceBinding::FragmentStage,
         atlasTexture_,
         atlasSampler_),
   });
   if (!geoSrb_->create())
   {
      logger_->error("Failed to create geo texture array shader bindings");
      return false;
   }

   if (screenSrb_ == nullptr)
   {
      screenSrb_ = rhi_->newShaderResourceBindings();
      if (screenSrb_ == nullptr)
      {
         logger_->error(
            "Failed to allocate screen texture array shader bindings");
         return false;
      }
   }

   screenSrb_->setBindings({
      QRhiShaderResourceBinding::uniformBuffer(
         0, QRhiShaderResourceBinding::VertexStage, screenUniformBuffer_),
      QRhiShaderResourceBinding::sampledTexture(
         1,
         QRhiShaderResourceBinding::FragmentStage,
         atlasTexture_,
         atlasSampler_),
   });
   if (!screenSrb_->create())
   {
      logger_->error("Failed to create screen texture array shader bindings");
      return false;
   }

   return true;
}

bool RhiTextureArrayOverlay::EnsureGeoPipeline(QRhi*             rhi,
                                               QRhiRenderTarget* renderTarget)
{
   geoPipeline_ = AcquireTextureArrayGeoPipeline(rhi, renderTarget);
   return geoPipeline_ != nullptr;
}

bool RhiTextureArrayOverlay::EnsureScreenPipeline(
   QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   screenPipeline_ = AcquireTextureArrayScreenPipeline(rhi, renderTarget);
   return screenPipeline_ != nullptr;
}

void RhiTextureArrayOverlay::Shutdown()
{
   // Pipelines and atlas texture/sampler are owned by the shared GPU store.
   geoPipeline_    = nullptr;
   screenPipeline_ = nullptr;
   atlasTexture_   = nullptr;
   atlasSampler_   = nullptr;
   delete geoSrb_;
   geoSrb_ = nullptr;
   delete screenSrb_;
   screenSrb_ = nullptr;
   delete screenTexCoordBuffer_;
   screenTexCoordBuffer_ = nullptr;
   delete screenFloatBuffer_;
   screenFloatBuffer_ = nullptr;
   delete screenUniformBuffer_;
   screenUniformBuffer_ = nullptr;
   delete geoIntegerBuffer_;
   geoIntegerBuffer_ = nullptr;
   delete geoFloatBuffer_;
   geoFloatBuffer_ = nullptr;
   delete geoUniformBuffer_;
   geoUniformBuffer_       = nullptr;
   rhi_                    = nullptr;
   renderTarget_           = nullptr;
   geoFloatCapacity_       = 0;
   geoIntegerCapacity_     = 0;
   screenFloatCapacity_    = 0;
   screenTexCoordCapacity_ = 0;
   atlasWidth_             = 0;
   atlasHeight_            = 0;
   atlasLayers_            = 0;
   syncedBuildCount_       = 0;
   initialized_            = false;
}

void RhiTextureArrayOverlay::SyncAtlas(QRhiCommandBuffer*       commandBuffer,
                                       const std::uint64_t      buildCount,
                                       QRhiResourceUpdateBatch* resourceBatch,
                                       RhiOverlayPhase          phase)
{
   if (!initialized_ || commandBuffer == nullptr || rhi_ == nullptr)
   {
      return;
   }

   const RhiSharedAtlas shared =
      AcquireSharedAtlas(rhi_, commandBuffer, buildCount, resourceBatch, phase);
   if (shared.texture_ == nullptr || shared.sampler_ == nullptr)
   {
      return;
   }

   const bool atlasIdentityChanged =
      atlasTexture_ != shared.texture_ || atlasSampler_ != shared.sampler_ ||
      atlasWidth_ != shared.width_ || atlasHeight_ != shared.height_ ||
      atlasLayers_ != shared.layers_;

   atlasTexture_     = shared.texture_;
   atlasSampler_     = shared.sampler_;
   atlasWidth_       = shared.width_;
   atlasHeight_      = shared.height_;
   atlasLayers_      = shared.layers_;
   syncedBuildCount_ = shared.buildCount_;

   if (atlasIdentityChanged)
   {
      delete geoSrb_;
      geoSrb_ = nullptr;
      delete screenSrb_;
      screenSrb_ = nullptr;
   }

   if (!EnsureShaderResources(rhi_) ||
       !EnsureGeoPipeline(rhi_, renderTarget_) ||
       !EnsureScreenPipeline(rhi_, renderTarget_))
   {
      logger_->error("Failed to bind shared texture array overlay resources");
   }
}

void RhiTextureArrayOverlay::RenderGeo(
   QRhiCommandBuffer*               commandBuffer,
   const GeoUniforms&               uniforms,
   const std::vector<float>&        floatVertices,
   const std::vector<std::int32_t>& integerVertices,
   const std::uint32_t              vertexCount,
   QRhiResourceUpdateBatch*         resourceBatch,
   RhiOverlayPhase                  phase)
{
   if (!initialized_ || commandBuffer == nullptr || vertexCount == 0 ||
       geoPipeline_ == nullptr || geoSrb_ == nullptr ||
       atlasTexture_ == nullptr)
   {
      return;
   }

   const std::size_t floatBytes = floatVertices.size() * sizeof(float);
   const std::size_t integerBytes =
      integerVertices.size() * sizeof(std::int32_t);

   if (OverlayShouldUpload(phase) && geoFloatCapacity_ < floatBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               geoFloatBuffer_,
                               geoFloatCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               floatBytes))
      {
         return;
      }
   }
   if (OverlayShouldUpload(phase) && geoIntegerCapacity_ < integerBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               geoIntegerBuffer_,
                               geoIntegerCapacity_,
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
      batch->updateDynamicBuffer(
         geoUniformBuffer_, 0, kGeoUniformBytes, &uniforms);
      batch->updateDynamicBuffer(geoFloatBuffer_,
                                 0,
                                 static_cast<quint32>(floatBytes),
                                 floatVertices.data());
      batch->updateDynamicBuffer(geoIntegerBuffer_,
                                 0,
                                 static_cast<quint32>(integerBytes),
                                 integerVertices.data());
      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   const QRhiCommandBuffer::VertexInput bindings[] = {{geoFloatBuffer_, 0},
                                                      {geoIntegerBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(geoPipeline_);
   commandBuffer->setShaderResources(geoSrb_);
   commandBuffer->setVertexInput(0, 2, bindings);
   commandBuffer->draw(vertexCount);
}

void RhiTextureArrayOverlay::RenderScreen(
   QRhiCommandBuffer*        commandBuffer,
   const glm::mat4&          projection,
   const std::vector<float>& floatVertices,
   const std::vector<float>& texCoords,
   const std::uint32_t       vertexCount,
   QRhiResourceUpdateBatch*  resourceBatch,
   RhiOverlayPhase           phase)
{
   if (!initialized_ || commandBuffer == nullptr || vertexCount == 0 ||
       screenPipeline_ == nullptr || screenSrb_ == nullptr ||
       atlasTexture_ == nullptr)
   {
      return;
   }

   const std::size_t floatBytes    = floatVertices.size() * sizeof(float);
   const std::size_t texCoordBytes = texCoords.size() * sizeof(float);

   if (OverlayShouldUpload(phase) && screenFloatCapacity_ < floatBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               screenFloatBuffer_,
                               screenFloatCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               floatBytes))
      {
         return;
      }
   }
   if (OverlayShouldUpload(phase) && screenTexCoordCapacity_ < texCoordBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               screenTexCoordBuffer_,
                               screenTexCoordCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               texCoordBytes))
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
      batch->updateDynamicBuffer(screenUniformBuffer_,
                                 0,
                                 kScreenUniformBytes,
                                 glm::value_ptr(projection));
      batch->updateDynamicBuffer(screenFloatBuffer_,
                                 0,
                                 static_cast<quint32>(floatBytes),
                                 floatVertices.data());
      batch->updateDynamicBuffer(screenTexCoordBuffer_,
                                 0,
                                 static_cast<quint32>(texCoordBytes),
                                 texCoords.data());
      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   const QRhiCommandBuffer::VertexInput bindings[] = {
      {screenFloatBuffer_, 0}, {screenTexCoordBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(screenPipeline_);
   commandBuffer->setShaderResources(screenSrb_);
   commandBuffer->setVertexInput(0, 2, bindings);
   commandBuffer->draw(vertexCount);
}

bool RhiTextureArrayOverlay::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::render
