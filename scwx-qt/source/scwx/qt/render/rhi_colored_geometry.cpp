#include <scwx/qt/render/rhi_colored_geometry.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_shader_util.hpp>
#include <scwx/util/logger.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_colored_geometry";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kUniformBytes = 64;

void RhiColoredGeometry::Initialize(QRhi*             rhi,
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

   if (!EnsurePipeline(rhi_, renderTarget_))
   {
      Shutdown();
      return;
   }

   initialized_ = true;
}

bool RhiColoredGeometry::EnsurePipeline(QRhi*             rhi,
                                        QRhiRenderTarget* renderTarget)
{
   if (pipeline_ != nullptr)
   {
      return true;
   }

   const QShader vertexShader =
      LoadSpirvShader(":/gl/vulkan/spirv/color.vert.spv", QShader::VertexStage);
   const QShader fragmentShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/color.frag.spv", QShader::FragmentStage);
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load colored geometry SPIR-V shaders");
      return false;
   }

   srb_ = rhi->newShaderResourceBindings();
   if (srb_ == nullptr)
   {
      logger_->error("Failed to allocate colored geometry shader bindings");
      return false;
   }
   srb_->setBindings({QRhiShaderResourceBinding::uniformBuffer(
      0, QRhiShaderResourceBinding::VertexStage, uniformBuffer_)});
   if (!srb_->create())
   {
      return false;
   }

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{7 * sizeof(float)}});
   inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float3, 0},
       {0, 1, QRhiVertexInputAttribute::Float4, 3 * sizeof(float)}});

   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      logger_->error("Failed to allocate colored geometry pipeline");
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
      logger_->error("Failed to create colored geometry pipeline");
      return false;
   }

   pipeline_ = pipeline.release();
   return true;
}

void RhiColoredGeometry::Shutdown()
{
   delete pipeline_;
   pipeline_ = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete vertexBuffer_;
   vertexBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_  = nullptr;
   rhi_            = nullptr;
   renderTarget_   = nullptr;
   vertexCapacity_ = 0;
   initialized_    = false;
}

void RhiColoredGeometry::Render(QRhiCommandBuffer*        commandBuffer,
                                const glm::mat4&          projection,
                                const std::vector<float>& vertices,
                                const std::size_t         vertexCount,
                                QRhiResourceUpdateBatch*  resourceBatch,
                                RhiOverlayPhase           phase)
{
   if (!initialized_ || commandBuffer == nullptr || vertexCount == 0)
   {
      return;
   }

   const std::size_t requiredBytes = vertexCount * 7 * sizeof(float);
   if (OverlayShouldUpload(phase) && vertexCapacity_ < requiredBytes)
   {
      if (!EnsureDynamicBuffer(rhi_,
                               vertexBuffer_,
                               vertexCapacity_,
                               QRhiBuffer::Dynamic,
                               QRhiBuffer::VertexBuffer,
                               requiredBytes))
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
         uniformBuffer_, 0, kUniformBytes, glm::value_ptr(projection));
      batch->updateDynamicBuffer(vertexBuffer_,
                                 0,
                                 static_cast<quint32>(requiredBytes),
                                 vertices.data());
      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   const QRhiCommandBuffer::VertexInput bindings[] = {{vertexBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(pipeline_);
   commandBuffer->setShaderResources(srb_);
   commandBuffer->setVertexInput(0, 1, bindings);
   commandBuffer->draw(static_cast<quint32>(vertexCount));
}

bool RhiColoredGeometry::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::render
