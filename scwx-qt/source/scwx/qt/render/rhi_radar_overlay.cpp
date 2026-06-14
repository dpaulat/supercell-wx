#include <scwx/qt/render/rhi_radar_overlay.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_shader_util.hpp>
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

bool RhiRadarOverlay::EnsurePipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   if (pipeline_ != nullptr)
   {
      return true;
   }

   const QShader vertexShader =
      LoadSpirvShader(":/gl/vulkan/spirv/radar.vert.spv", QShader::VertexStage);
   const QShader fragmentShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/radar.frag.spv", QShader::FragmentStage);
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load radar SPIR-V shaders");
      return false;
   }

   lutTexture_ = rhi->newTexture(QRhiTexture::RGBA8, QSize(kMaxLutWidth, 1));
   if (lutTexture_ == nullptr || !lutTexture_->create())
   {
      logger_->error("Failed to create radar LUT texture");
      return false;
   }

   srb_ = rhi->newShaderResourceBindings();
   if (srb_ == nullptr)
   {
      logger_->error("Failed to allocate radar shader bindings");
      return false;
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

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings(
      {{2 * sizeof(float)}, {sizeof(std::uint32_t)}, {sizeof(std::uint32_t)}});
   inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                              {1, 1, QRhiVertexInputAttribute::UInt, 0},
                              {2, 2, QRhiVertexInputAttribute::UInt, 0}});

   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      logger_->error("Failed to allocate radar graphics pipeline");
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
      logger_->error("Failed to create radar graphics pipeline");
      return false;
   }

   pipeline_ = pipeline.release();
   return true;
}

void RhiRadarOverlay::Shutdown()
{
   delete pipeline_;
   pipeline_ = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete lutTexture_;
   lutTexture_ = nullptr;
   delete sampler_;
   sampler_ = nullptr;
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
   initialized_ = false;
}

void RhiRadarOverlay::Render(QRhiCommandBuffer*               commandBuffer,
                             const RadarUniforms&             uniforms,
                             const std::vector<float>&        vertices,
                             const std::vector<std::uint8_t>& momentData,
                             const std::size_t momentComponentSize,
                             const std::vector<std::uint8_t>& cfpData,
                             const std::size_t                cfpComponentSize,
                             const std::vector<std::uint8_t>& rgbaColorTable,
                             const std::uint32_t              vertexCount)
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

   ExpandMoments(momentData, momentComponentSize, vertexCount, momentU32_);
   ExpandMoments(cfpData, cfpComponentSize, vertexCount, cfpU32_);

   lutRgba_.resize(kMaxLutWidth * 4);
   for (int i = 0; i < kMaxLutWidth; ++i)
   {
      const int sourceIndex =
         (tableWidth == 1) ?
            0 :
            static_cast<int>((static_cast<std::int64_t>(i) *
                              static_cast<std::int64_t>(tableWidth - 1)) /
                             static_cast<std::int64_t>(kMaxLutWidth - 1));
      std::copy_n(
         rgbaColorTable.data() + sourceIndex * 4, 4, lutRgba_.data() + i * 4);
   }

   const std::size_t vertexBytes = vertexCount * 2u * sizeof(float);
   const std::size_t momentBytes = vertexCount * sizeof(std::uint32_t);

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

   QRhiResourceUpdateBatch* batch = rhi_->nextResourceUpdateBatch();
   batch->updateDynamicBuffer(uniformBuffer_, 0, kUniformBytes, &uniforms);
   batch->updateDynamicBuffer(
      vertexBuffer_, 0, static_cast<quint32>(vertexBytes), vertices.data());
   batch->updateDynamicBuffer(
      momentBuffer_, 0, static_cast<quint32>(momentBytes), momentU32_.data());
   batch->updateDynamicBuffer(
      cfpBuffer_, 0, static_cast<quint32>(momentBytes), cfpU32_.data());

   const QRhiTextureSubresourceUploadDescription subUpload(
      lutRgba_.data(), static_cast<quint32>(kMaxLutWidth * 4));
   batch->uploadTexture(
      lutTexture_,
      QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, subUpload)));

   commandBuffer->resourceUpdate(batch);

   const QRhiCommandBuffer::VertexInput bindings[] = {
      {vertexBuffer_, 0}, {momentBuffer_, 0}, {cfpBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(pipeline_);
   commandBuffer->setShaderResources(srb_);
   commandBuffer->setVertexInput(0, 3, bindings);
   commandBuffer->draw(vertexCount);
}

bool RhiRadarOverlay::IsInitialized() const
{ return initialized_; }

} // namespace scwx::qt::render
