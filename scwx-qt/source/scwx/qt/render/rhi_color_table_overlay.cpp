#include <scwx/qt/render/rhi_color_table_overlay.hpp>
#include <scwx/qt/render/rhi_shader_util.hpp>
#include <scwx/util/logger.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ =
   "scwx::qt::render::rhi_color_table_overlay";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr int kUniformBytes = 64;
static constexpr int kMaxLutWidth  = 512;

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

   sampler_ = rhi_->newSampler(QRhiSampler::Nearest,
                               QRhiSampler::Nearest,
                               QRhiSampler::None,
                               QRhiSampler::ClampToEdge,
                               QRhiSampler::ClampToEdge);
   if (sampler_ == nullptr || !sampler_->create())
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
   if (pipeline_ != nullptr)
   {
      return true;
   }

   const QShader vertexShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/texture1d.vert.spv", QShader::VertexStage);
   const QShader fragmentShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/texture_lut.frag.spv", QShader::FragmentStage);
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load color table SPIR-V shaders");
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

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{2 * sizeof(float)}, {sizeof(float)}});
   inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                              {1, 1, QRhiVertexInputAttribute::Float, 0}});

   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      logger_->error("Failed to allocate color table graphics pipeline");
      return false;
   }
   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   pipeline->setShaderResourceBindings(srb_);
   pipeline->setRenderPassDescriptor(renderTarget->renderPassDescriptor());
   pipeline->setSampleCount(renderTarget->sampleCount());
   pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
   pipeline->setCullMode(QRhiGraphicsPipeline::None);
   pipeline->setDepthTest(false);
   pipeline->setDepthWrite(false);
   pipeline->setTargetBlends({blend});

   if (!pipeline->create())
   {
      logger_->error("Failed to create color table graphics pipeline");
      return false;
   }

   pipeline_ = pipeline.release();
   return true;
}

void RhiColorTableOverlay::Shutdown()
{
   delete pipeline_;
   pipeline_ = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete lutTexture_;
   lutTexture_ = nullptr;
   delete sampler_;
   sampler_ = nullptr;
   delete texCoordBuffer_;
   texCoordBuffer_ = nullptr;
   delete vertexBuffer_;
   vertexBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_ = nullptr;
   rhi_           = nullptr;
   renderTarget_  = nullptr;
   lutWidth_      = 0;
   initialized_   = false;
}

void RhiColorTableOverlay::Render(
   QRhiCommandBuffer*               commandBuffer,
   const glm::mat4&                 projection,
   const float                      vertices[6][2],
   const std::vector<std::uint8_t>& rgbaColorTable)
{
   if (!initialized_ || commandBuffer == nullptr || rgbaColorTable.empty())
   {
      return;
   }

   const int tableWidth = static_cast<int>(rgbaColorTable.size() / 4);
   if (tableWidth <= 0 || tableWidth > kMaxLutWidth)
   {
      return;
   }

   QRhiResourceUpdateBatch* batch = rhi_->nextResourceUpdateBatch();
   batch->updateDynamicBuffer(
      uniformBuffer_, 0, kUniformBytes, glm::value_ptr(projection));
   batch->updateDynamicBuffer(
      vertexBuffer_, 0, sizeof(float) * 6 * 2, vertices);

   const QRhiTextureSubresourceUploadDescription subUpload(
      rgbaColorTable.data(), static_cast<quint32>(tableWidth * 4));
   const QRhiTextureUploadDescription upload(
      QRhiTextureUploadEntry(0, 0, subUpload));
   batch->uploadTexture(lutTexture_, upload);

   commandBuffer->resourceUpdate(batch);

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
