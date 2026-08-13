#include <scwx/qt/render/rhi_colored_geometry.hpp>
#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_buffer_util.hpp>
#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>
#include <scwx/util/logger.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_colored_geometry";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int         kUniformBytes    = 64;
static constexpr std::size_t kFloatsPerVertex = 7;

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

bool RhiColoredGeometry::BindResources()
{
   if (srb_ == nullptr || uniformBuffer_ == nullptr)
   {
      return false;
   }

   if (backdropTexture_ == nullptr || backdropSampler_ == nullptr)
   {
      logger_->error("Colored geometry missing backdrop for alpha composite");
      return false;
   }

   srb_->setBindings({QRhiShaderResourceBinding::uniformBuffer(
                         0,
                         QRhiShaderResourceBinding::VertexStage |
                            QRhiShaderResourceBinding::FragmentStage,
                         uniformBuffer_),
                      QRhiShaderResourceBinding::sampledTexture(
                         1,
                         QRhiShaderResourceBinding::FragmentStage,
                         backdropTexture_,
                         backdropSampler_)});
   return srb_->create();
}

bool RhiColoredGeometry::EnsurePipeline(QRhi*             rhi,
                                        QRhiRenderTarget* renderTarget)
{
   pipeline_ = AcquireColoredGeometryPipeline(rhi, renderTarget);
   if (pipeline_ == nullptr)
   {
      return false;
   }

   if (srb_ == nullptr)
   {
      srb_ = rhi->newShaderResourceBindings();
      if (srb_ == nullptr)
      {
         logger_->error("Failed to allocate colored geometry shader bindings");
         return false;
      }
   }

   // Backdrop may be bound later via SetBackdrop before the first Draw.
   if (backdropTexture_ != nullptr && backdropSampler_ != nullptr &&
       !BindResources())
   {
      return false;
   }

   return true;
}

void RhiColoredGeometry::SetBackdrop(QRhiTexture* texture, QRhiSampler* sampler)
{
   backdropTexture_ = texture;
   backdropSampler_ = sampler;
   if (initialized_ && srb_ != nullptr)
   {
      static_cast<void>(BindResources());
   }
}

void RhiColoredGeometry::Shutdown()
{
   // Pipeline owned by shared GPU store. Sampler owned by store.
   pipeline_        = nullptr;
   backdropTexture_ = nullptr;
   backdropSampler_ = nullptr;
   delete srb_;
   srb_ = nullptr;
   delete vertexBuffer_;
   vertexBuffer_ = nullptr;
   delete uniformBuffer_;
   uniformBuffer_  = nullptr;
   rhi_            = nullptr;
   renderTarget_   = nullptr;
   vertexCapacity_ = 0;
   stagingVertices_.clear();
   drawCommands_.clear();
   drawCursor_    = 0;
   frameUploaded_ = false;
   initialized_   = false;
}

void RhiColoredGeometry::BeginFrame()
{
   stagingVertices_.clear();
   drawCommands_.clear();
   drawCursor_    = 0;
   frameUploaded_ = false;
}

void RhiColoredGeometry::AppendTransformed(const glm::mat4&          projection,
                                           const std::vector<float>& vertices,
                                           std::size_t vertexCount)
{
   stagingVertices_.reserve(stagingVertices_.size() +
                            vertexCount * kFloatsPerVertex);
   for (std::size_t i = 0; i < vertexCount; ++i)
   {
      const float*    v    = vertices.data() + i * kFloatsPerVertex;
      const glm::vec4 clip = projection * glm::vec4 {v[0], v[1], v[2], 1.0f};
      // Shader: uMVP * vec4(aVertex, 1). Bake MVP here; UBO stays identity.
      stagingVertices_.push_back(clip.x);
      stagingVertices_.push_back(clip.y);
      stagingVertices_.push_back(clip.z);
      stagingVertices_.push_back(v[3]);
      stagingVertices_.push_back(v[4]);
      stagingVertices_.push_back(v[5]);
      stagingVertices_.push_back(v[6]);
   }
}

void RhiColoredGeometry::UploadFrame(QRhiResourceUpdateBatch* resourceBatch)
{
   if (!initialized_ || frameUploaded_ || resourceBatch == nullptr ||
       rhi_ == nullptr)
   {
      return;
   }

   frameUploaded_ = true;

   static const glm::mat4 kIdentity {1.0f};
   resourceBatch->updateDynamicBuffer(
      uniformBuffer_, 0, kUniformBytes, glm::value_ptr(kIdentity));

   if (stagingVertices_.empty())
   {
      return;
   }

   const std::size_t requiredBytes = stagingVertices_.size() * sizeof(float);
   if (vertexCapacity_ < requiredBytes)
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

   resourceBatch->updateDynamicBuffer(vertexBuffer_,
                                      0,
                                      static_cast<quint32>(requiredBytes),
                                      stagingVertices_.data());
}

void RhiColoredGeometry::Render(QRhiCommandBuffer*        commandBuffer,
                                const glm::mat4&          projection,
                                const std::vector<float>& vertices,
                                const std::size_t         vertexCount,
                                QRhiResourceUpdateBatch*  resourceBatch,
                                RhiOverlayPhase           phase)
{
   if (!initialized_ || commandBuffer == nullptr)
   {
      return;
   }

   // Draw-only phase consumes recorded commands; vertex payload may be ignored.
   // Upload still requires non-empty geometry.
   if (OverlayShouldUpload(phase) && vertexCount == 0)
   {
      return;
   }

   if (OverlayShouldUpload(phase))
   {
      const std::size_t floatCount = vertexCount * kFloatsPerVertex;
      if (vertices.size() < floatCount)
      {
         logger_->error(
            "Colored geometry vertex buffer shorter than vertexCount");
         return;
      }

      // Single-pass callers never invoke BeginFrame()/UploadFrame().
      if (phase == RhiOverlayPhase::UploadAndDraw)
      {
         BeginFrame();
      }

      const std::size_t firstVertex =
         stagingVertices_.size() / kFloatsPerVertex;
      AppendTransformed(projection, vertices, vertexCount);
      drawCommands_.push_back(DrawCommand {firstVertex, vertexCount});

      if (phase == RhiOverlayPhase::UploadAndDraw)
      {
         QRhiResourceUpdateBatch* batch =
            AcquireOverlayBatch(rhi_, resourceBatch, phase);
         if (batch == nullptr)
         {
            return;
         }
         UploadFrame(batch);
         SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
      }
   }

   if (!OverlayShouldDraw(phase))
   {
      return;
   }

   if (backdropTexture_ == nullptr || backdropSampler_ == nullptr ||
       srb_ == nullptr || pipeline_ == nullptr)
   {
      logger_->error(
         "Colored geometry draw skipped: missing backdrop/pipeline");
      return;
   }

   if (drawCursor_ >= drawCommands_.size())
   {
      logger_->error("Colored geometry draw queue underrun");
      return;
   }

   const DrawCommand& cmd = drawCommands_[drawCursor_++];

   const QRhiCommandBuffer::VertexInput bindings[] = {{vertexBuffer_, 0}};
   commandBuffer->setGraphicsPipeline(pipeline_);
   commandBuffer->setShaderResources(srb_);
   commandBuffer->setVertexInput(0, 1, bindings);
   commandBuffer->draw(static_cast<quint32>(cmd.vertexCount),
                       1,
                       static_cast<quint32>(cmd.firstVertex));
}

bool RhiColoredGeometry::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::render
