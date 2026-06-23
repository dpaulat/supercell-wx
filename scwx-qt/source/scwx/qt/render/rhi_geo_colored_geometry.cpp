#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_shader_util.hpp>
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
   if (pipeline_ != nullptr)
   {
      return true;
   }

   const QShader vertexShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/geo_color.vert.spv", QShader::VertexStage);
   const QShader fragmentShader = LoadSpirvShader(
      ":/gl/vulkan/spirv/geo_color.frag.spv", QShader::FragmentStage);
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load geo colored SPIR-V shaders");
      return false;
   }

   srb_ = rhi->newShaderResourceBindings();
   if (srb_ == nullptr)
   {
      logger_->error("Failed to allocate geo colored shader bindings");
      return false;
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

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{20 * sizeof(float)}, {4 * sizeof(std::int32_t)}});
   inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float2, 0},
       {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
       {0, 2, QRhiVertexInputAttribute::Float4, 4 * sizeof(float)},
       {0, 3, QRhiVertexInputAttribute::Float, 8 * sizeof(float)},
       {0, 4, QRhiVertexInputAttribute::Float4, 9 * sizeof(float)},
       {0, 5, QRhiVertexInputAttribute::Float4, 13 * sizeof(float)},
       {0, 6, QRhiVertexInputAttribute::Float3, 17 * sizeof(float)},
       {1, 7, QRhiVertexInputAttribute::SInt, 0},
       {1, 8, QRhiVertexInputAttribute::SInt, sizeof(std::int32_t)},
       {1, 9, QRhiVertexInputAttribute::SInt, 2 * sizeof(std::int32_t)},
       {1, 10, QRhiVertexInputAttribute::SInt, 3 * sizeof(std::int32_t)}});

   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      logger_->error("Failed to allocate geo colored pipeline");
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
      logger_->error("Failed to create geo colored pipeline");
      return false;
   }

   pipeline_ = pipeline.release();
   return true;
}

void RhiGeoColoredGeometry::Shutdown()
{
   delete pipeline_;
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
   bool                             uploadGeometry)
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

   if (uploadGeometry && floatCapacity_ < floatBytes)
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
   if (uploadGeometry && integerCapacity_ < integerBytes)
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

   QRhiResourceUpdateBatch* batch = rhi_->nextResourceUpdateBatch();
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
   commandBuffer->resourceUpdate(batch);

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
