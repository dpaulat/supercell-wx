#include <scwx/qt/render/rhi_texture_array_overlay.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_shader_util.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
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

static constexpr std::size_t kGeoFloatsPerVertex       = 12;
static constexpr std::size_t kGeoIntegersPerVertex     = 4;
static constexpr std::size_t kScreenFloatsPerVertex    = 10;
static constexpr std::size_t kScreenTexCoordsPerVertex = 3;

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

   atlasSampler_ = rhi_->newSampler(QRhiSampler::Linear,
                                    QRhiSampler::Linear,
                                    QRhiSampler::None,
                                    QRhiSampler::ClampToEdge,
                                    QRhiSampler::ClampToEdge,
                                    QRhiSampler::ClampToEdge);
   if (atlasSampler_ == nullptr || !atlasSampler_->create())
   {
      Shutdown();
      return;
   }

   initialized_ = true;
}

bool RhiTextureArrayOverlay::EnsureShaderResources(QRhi* rhi)
{
   if (atlasTexture_ == nullptr)
   {
      return false;
   }

   if (geoSrb_ == nullptr)
   {
      geoSrb_ = rhi->newShaderResourceBindings();
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
      screenSrb_ = rhi->newShaderResourceBindings();
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
   if (geoPipeline_ != nullptr)
   {
      return true;
   }

   const QShader vertexShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/geo_texture_array.vert.spv", QShader::VertexStage);
   const QShader fragmentShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/geo_texture_array.frag.spv", QShader::FragmentStage);
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load geo texture array SPIR-V shaders");
      return false;
   }

   if (atlasTexture_ != nullptr && !EnsureShaderResources(rhi))
   {
      return false;
   }

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{kGeoFloatsPerVertex * sizeof(float)},
                            {kGeoIntegersPerVertex * sizeof(std::int32_t)}});
   inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float2, 0},
       {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
       {0, 2, QRhiVertexInputAttribute::Float3, 4 * sizeof(float)},
       {0, 3, QRhiVertexInputAttribute::Float4, 7 * sizeof(float)},
       {0, 4, QRhiVertexInputAttribute::Float, 11 * sizeof(float)},
       {1, 5, QRhiVertexInputAttribute::SInt, 0},
       {1, 6, QRhiVertexInputAttribute::SInt, sizeof(std::int32_t)},
       {1, 7, QRhiVertexInputAttribute::SInt, 2 * sizeof(std::int32_t)},
       {1, 8, QRhiVertexInputAttribute::SInt, 3 * sizeof(std::int32_t)}});

   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      logger_->error("Failed to allocate geo texture array pipeline");
      return false;
   }
   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (geoSrb_ != nullptr)
   {
      pipeline->setShaderResourceBindings(geoSrb_);
   }
   pipeline->setRenderPassDescriptor(renderTarget->renderPassDescriptor());
   pipeline->setSampleCount(renderTarget->sampleCount());
   pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
   pipeline->setCullMode(QRhiGraphicsPipeline::None);
   pipeline->setDepthTest(false);
   pipeline->setDepthWrite(false);
   pipeline->setTargetBlends({blend});

   if (!pipeline->create())
   {
      logger_->error("Failed to create geo texture array pipeline");
      return false;
   }

   geoPipeline_ = pipeline.release();
   return true;
}

bool RhiTextureArrayOverlay::EnsureScreenPipeline(
   QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   if (screenPipeline_ != nullptr)
   {
      return true;
   }

   const QShader vertexShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/screen_texture_array.vert.spv", QShader::VertexStage);
   const QShader fragmentShader =
      LoadSpirvShader(":/gl/vulkan/spirv/screen_texture_array.frag.spv",
                      QShader::FragmentStage);
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load screen texture array SPIR-V shaders");
      return false;
   }

   if (atlasTexture_ != nullptr && !EnsureShaderResources(rhi))
   {
      return false;
   }

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{kScreenFloatsPerVertex * sizeof(float)},
                            {kScreenTexCoordsPerVertex * sizeof(float)}});
   inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float2, 0},
       {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
       {0, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float)},
       {0, 4, QRhiVertexInputAttribute::Float, 8 * sizeof(float)},
       {0, 5, QRhiVertexInputAttribute::Float, 9 * sizeof(float)},
       {1, 2, QRhiVertexInputAttribute::Float3, 0}});

   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      logger_->error("Failed to allocate screen texture array pipeline");
      return false;
   }
   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (screenSrb_ != nullptr)
   {
      pipeline->setShaderResourceBindings(screenSrb_);
   }
   pipeline->setRenderPassDescriptor(renderTarget->renderPassDescriptor());
   pipeline->setSampleCount(renderTarget->sampleCount());
   pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
   pipeline->setCullMode(QRhiGraphicsPipeline::None);
   pipeline->setDepthTest(false);
   pipeline->setDepthWrite(false);
   pipeline->setTargetBlends({blend});

   if (!pipeline->create())
   {
      logger_->error("Failed to create screen texture array pipeline");
      return false;
   }

   screenPipeline_ = pipeline.release();
   return true;
}

void RhiTextureArrayOverlay::Shutdown()
{
   delete geoPipeline_;
   geoPipeline_ = nullptr;
   delete screenPipeline_;
   screenPipeline_ = nullptr;
   delete geoSrb_;
   geoSrb_ = nullptr;
   delete screenSrb_;
   screenSrb_ = nullptr;
   delete atlasTexture_;
   atlasTexture_ = nullptr;
   delete atlasSampler_;
   atlasSampler_ = nullptr;
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
   if (!initialized_ || commandBuffer == nullptr)
   {
      return;
   }

   util::TextureAtlas& atlas  = util::TextureAtlas::Instance();
   const std::size_t   layers = atlas.LayerCount();
   const std::size_t   width  = atlas.AtlasWidth();
   const std::size_t   height = atlas.AtlasHeight();

   if (layers == 0 || width == 0 || height == 0)
   {
      return;
   }

   const bool atlasChanged = atlasTexture_ == nullptr || atlasWidth_ != width ||
                             atlasHeight_ != height || atlasLayers_ != layers ||
                             syncedBuildCount_ != buildCount;

   if (!atlasChanged)
   {
      return;
   }

   if (!OverlayShouldUpload(phase))
   {
      return;
   }

   if (atlasTexture_ == nullptr || atlasWidth_ != width ||
       atlasHeight_ != height || atlasLayers_ != layers)
   {
      delete geoPipeline_;
      geoPipeline_ = nullptr;
      delete screenPipeline_;
      screenPipeline_ = nullptr;
      delete geoSrb_;
      geoSrb_ = nullptr;
      delete screenSrb_;
      screenSrb_ = nullptr;
      delete atlasTexture_;
      atlasTexture_ = rhi_->newTextureArray(
         QRhiTexture::RGBA8,
         static_cast<int>(layers),
         QSize(static_cast<int>(width), static_cast<int>(height)));
      if (atlasTexture_ == nullptr || !atlasTexture_->create())
      {
         logger_->error("Failed to create texture atlas array");
         delete atlasTexture_;
         atlasTexture_ = nullptr;
         return;
      }

      atlasWidth_  = width;
      atlasHeight_ = height;
      atlasLayers_ = layers;

      if (!EnsureShaderResources(rhi_) ||
          !EnsureGeoPipeline(rhi_, renderTarget_) ||
          !EnsureScreenPipeline(rhi_, renderTarget_))
      {
         logger_->error("Failed to rebuild texture array overlay pipelines");
         return;
      }
   }

   QRhiResourceUpdateBatch* batch =
      AcquireOverlayBatch(rhi_, resourceBatch, phase);
   if (batch == nullptr)
   {
      return;
   }

   for (std::size_t layer = 0; layer < layers; ++layer)
   {
      const std::vector<std::uint8_t> pixels = atlas.CopyLayerPixels(layer);
      if (pixels.empty())
      {
         continue;
      }

      const QRhiTextureSubresourceUploadDescription subUpload(
         pixels.data(), static_cast<quint32>(pixels.size()));
      batch->uploadTexture(atlasTexture_,
                           QRhiTextureUploadDescription(QRhiTextureUploadEntry(
                              static_cast<int>(layer), 0, subUpload)));
   }

   SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   syncedBuildCount_ = buildCount;
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
