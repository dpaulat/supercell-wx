#include <scwx/qt/render/rhi_overlay_gpu_store.hpp>

#include <scwx/qt/render/rhi_overlay_util.hpp>
#include <scwx/qt/render/rhi_shader_util.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <mutex>
#include <unordered_map>
#include <vector>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

namespace scwx::qt::render
{

namespace
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_overlay_gpu_store";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kGeoUniformBytes = 144;

static constexpr std::size_t kGeoFloatsPerVertex       = 12;
static constexpr std::size_t kGeoIntegersPerVertex     = 4;
static constexpr std::size_t kScreenFloatsPerVertex    = 10;
static constexpr std::size_t kScreenTexCoordsPerVertex = 3;

[[nodiscard]] void* NativeRenderPass(QRhiRenderTarget* renderTarget)
{
   if (renderTarget == nullptr)
   {
      return nullptr;
   }

   QRhiRenderPassDescriptor* renderPassDescriptor =
      renderTarget->renderPassDescriptor();
   if (renderPassDescriptor == nullptr)
   {
      return nullptr;
   }

#if !defined(__APPLE__)
   const QRhiNativeHandles* nativeHandles =
      renderPassDescriptor->nativeHandles();
   if (nativeHandles != nullptr)
   {
      const auto* vkHandles =
         static_cast<const QRhiVulkanRenderPassNativeHandles*>(nativeHandles);
      if (vkHandles != nullptr && vkHandles->renderPass != nullptr)
      {
         return static_cast<void*>(vkHandles->renderPass);
      }
   }
#endif

   return static_cast<void*>(renderPassDescriptor);
}

struct PipelineKey
{
   void* nativeRenderPass_ {nullptr};
   int   sampleCount_ {1};

   [[nodiscard]] bool operator==(const PipelineKey& other) const noexcept
   {
      return nativeRenderPass_ == other.nativeRenderPass_ &&
             sampleCount_ == other.sampleCount_;
   }
};

struct PipelineKeyHash
{
   [[nodiscard]] std::size_t operator()(const PipelineKey& key) const noexcept
   {
      return std::hash<void*> {}(key.nativeRenderPass_) ^
             (std::hash<int> {}(key.sampleCount_) << 1);
   }
};

struct PipelineBucket
{
   QRhiGraphicsPipeline* coloredGeometry_ {nullptr};
   QRhiGraphicsPipeline* geoColoredGeometry_ {nullptr};
   QRhiGraphicsPipeline* radar_ {nullptr};
   QRhiGraphicsPipeline* colorTable_ {nullptr};
   QRhiGraphicsPipeline* textureArrayGeo_ {nullptr};
   QRhiGraphicsPipeline* textureArrayScreen_ {nullptr};

   QRhiBuffer*                 layoutUniform_ {nullptr};
   QRhiTexture*                layoutTexture_ {nullptr};
   QRhiSampler*                layoutSampler_ {nullptr};
   QRhiShaderResourceBindings* layoutSrbUniform_ {nullptr};
   QRhiShaderResourceBindings* layoutSrbUniformTexture_ {nullptr};
   QRhiShaderResourceBindings* layoutSrbGeoTexture_ {nullptr};
   QRhiShaderResourceBindings* layoutSrbScreenTexture_ {nullptr};
   QRhiShaderResourceBindings* layoutSrbGeoUniform_ {nullptr};

   void Destroy()
   {
      delete coloredGeometry_;
      coloredGeometry_ = nullptr;
      delete geoColoredGeometry_;
      geoColoredGeometry_ = nullptr;
      delete radar_;
      radar_ = nullptr;
      delete colorTable_;
      colorTable_ = nullptr;
      delete textureArrayGeo_;
      textureArrayGeo_ = nullptr;
      delete textureArrayScreen_;
      textureArrayScreen_ = nullptr;
      delete layoutSrbUniform_;
      layoutSrbUniform_ = nullptr;
      delete layoutSrbUniformTexture_;
      layoutSrbUniformTexture_ = nullptr;
      delete layoutSrbGeoTexture_;
      layoutSrbGeoTexture_ = nullptr;
      delete layoutSrbScreenTexture_;
      layoutSrbScreenTexture_ = nullptr;
      delete layoutSrbGeoUniform_;
      layoutSrbGeoUniform_ = nullptr;
      delete layoutUniform_;
      layoutUniform_ = nullptr;
      delete layoutTexture_;
      layoutTexture_ = nullptr;
      delete layoutSampler_;
      layoutSampler_ = nullptr;
   }
};

struct RhiStore
{
   std::unordered_map<PipelineKey, PipelineBucket, PipelineKeyHash>
                 pipelines_ {};
   QRhiTexture*  atlasTexture_ {nullptr};
   QRhiSampler*  atlasSampler_ {nullptr};
   QRhiSampler*  nearestSampler_ {nullptr};
   std::size_t   atlasWidth_ {0};
   std::size_t   atlasHeight_ {0};
   std::size_t   atlasLayers_ {0};
   std::uint64_t atlasBuildCount_ {0};
   std::size_t   retainCount_ {0};

   void Destroy()
   {
      for (auto& [key, bucket] : pipelines_)
      {
         (void) key;
         bucket.Destroy();
      }
      pipelines_.clear();
      delete atlasTexture_;
      atlasTexture_ = nullptr;
      delete atlasSampler_;
      atlasSampler_ = nullptr;
      delete nearestSampler_;
      nearestSampler_  = nullptr;
      atlasWidth_      = 0;
      atlasHeight_     = 0;
      atlasLayers_     = 0;
      atlasBuildCount_ = 0;
      retainCount_     = 0;
   }
};

std::mutex                          storeMutex_;
std::unordered_map<QRhi*, RhiStore> stores_;

[[nodiscard]] RhiStore& GetStore(QRhi* rhi)
{
   return stores_[rhi];
}

[[nodiscard]] PipelineKey MakeKey(QRhiRenderTarget* renderTarget)
{
   return PipelineKey {.nativeRenderPass_ = NativeRenderPass(renderTarget),
                       .sampleCount_      = renderTarget != nullptr ?
                                               renderTarget->sampleCount() :
                                               1};
}

[[nodiscard]] bool EnsureLayoutResources(QRhi* rhi, PipelineBucket& bucket)
{
   if (bucket.layoutUniform_ == nullptr)
   {
      bucket.layoutUniform_ = rhi->newBuffer(
         QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kGeoUniformBytes);
      if (bucket.layoutUniform_ == nullptr || !bucket.layoutUniform_->create())
      {
         return false;
      }
   }

   if (bucket.layoutTexture_ == nullptr)
   {
      bucket.layoutTexture_ = rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1));
      if (bucket.layoutTexture_ == nullptr || !bucket.layoutTexture_->create())
      {
         return false;
      }
   }

   if (bucket.layoutSampler_ == nullptr)
   {
      bucket.layoutSampler_ = rhi->newSampler(QRhiSampler::Nearest,
                                              QRhiSampler::Nearest,
                                              QRhiSampler::None,
                                              QRhiSampler::ClampToEdge,
                                              QRhiSampler::ClampToEdge);
      if (bucket.layoutSampler_ == nullptr || !bucket.layoutSampler_->create())
      {
         return false;
      }
   }

   if (bucket.layoutSrbUniform_ == nullptr)
   {
      bucket.layoutSrbUniform_ = rhi->newShaderResourceBindings();
      if (bucket.layoutSrbUniform_ == nullptr)
      {
         return false;
      }
      bucket.layoutSrbUniform_->setBindings(
         {QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage, bucket.layoutUniform_)});
      if (!bucket.layoutSrbUniform_->create())
      {
         return false;
      }
   }

   if (bucket.layoutSrbGeoUniform_ == nullptr)
   {
      bucket.layoutSrbGeoUniform_ = rhi->newShaderResourceBindings();
      if (bucket.layoutSrbGeoUniform_ == nullptr)
      {
         return false;
      }
      bucket.layoutSrbGeoUniform_->setBindings(
         {QRhiShaderResourceBinding::uniformBuffer(
            0,
            QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
            bucket.layoutUniform_)});
      if (!bucket.layoutSrbGeoUniform_->create())
      {
         return false;
      }
   }

   if (bucket.layoutSrbUniformTexture_ == nullptr)
   {
      bucket.layoutSrbUniformTexture_ = rhi->newShaderResourceBindings();
      if (bucket.layoutSrbUniformTexture_ == nullptr)
      {
         return false;
      }
      bucket.layoutSrbUniformTexture_->setBindings({
         QRhiShaderResourceBinding::uniformBuffer(
            0,
            QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
            bucket.layoutUniform_),
         QRhiShaderResourceBinding::sampledTexture(
            1,
            QRhiShaderResourceBinding::FragmentStage,
            bucket.layoutTexture_,
            bucket.layoutSampler_),
      });
      if (!bucket.layoutSrbUniformTexture_->create())
      {
         return false;
      }
   }

   if (bucket.layoutSrbGeoTexture_ == nullptr)
   {
      bucket.layoutSrbGeoTexture_ = rhi->newShaderResourceBindings();
      if (bucket.layoutSrbGeoTexture_ == nullptr)
      {
         return false;
      }
      bucket.layoutSrbGeoTexture_->setBindings({
         QRhiShaderResourceBinding::uniformBuffer(
            0,
            QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
            bucket.layoutUniform_),
         QRhiShaderResourceBinding::sampledTexture(
            1,
            QRhiShaderResourceBinding::FragmentStage,
            bucket.layoutTexture_,
            bucket.layoutSampler_),
      });
      if (!bucket.layoutSrbGeoTexture_->create())
      {
         return false;
      }
   }

   if (bucket.layoutSrbScreenTexture_ == nullptr)
   {
      bucket.layoutSrbScreenTexture_ = rhi->newShaderResourceBindings();
      if (bucket.layoutSrbScreenTexture_ == nullptr)
      {
         return false;
      }
      bucket.layoutSrbScreenTexture_->setBindings({
         QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage, bucket.layoutUniform_),
         QRhiShaderResourceBinding::sampledTexture(
            1,
            QRhiShaderResourceBinding::FragmentStage,
            bucket.layoutTexture_,
            bucket.layoutSampler_),
      });
      if (!bucket.layoutSrbScreenTexture_->create())
      {
         return false;
      }
   }

   return true;
}

[[nodiscard]] QRhiGraphicsPipeline::TargetBlend AlphaBlend()
{
   QRhiGraphicsPipeline::TargetBlend blend;
   blend.enable   = true;
   blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
   blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   blend.srcAlpha = QRhiGraphicsPipeline::One;
   blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
   return blend;
}

[[nodiscard]] bool ConfigureCommonPipeline(QRhiGraphicsPipeline* pipeline,
                                           QRhiRenderTarget*     renderTarget,
                                           QRhiShaderResourceBindings* srb)
{
   if (pipeline == nullptr || renderTarget == nullptr || srb == nullptr)
   {
      return false;
   }

   pipeline->setShaderResourceBindings(srb);
   pipeline->setRenderPassDescriptor(renderTarget->renderPassDescriptor());
   pipeline->setSampleCount(renderTarget->sampleCount());
   pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
   pipeline->setCullMode(QRhiGraphicsPipeline::None);
   pipeline->setDepthTest(false);
   pipeline->setDepthWrite(false);
   pipeline->setTargetBlends({AlphaBlend()});
   return pipeline->create();
}

} // namespace

RhiSharedAtlas AcquireSharedAtlas(QRhi*                    rhi,
                                  QRhiCommandBuffer*       commandBuffer,
                                  const std::uint64_t      buildCount,
                                  QRhiResourceUpdateBatch* resourceBatch,
                                  const RhiOverlayPhase    phase)
{
   RhiSharedAtlas result {};
   if (rhi == nullptr)
   {
      return result;
   }

   std::lock_guard lock {storeMutex_};
   RhiStore&       store = GetStore(rhi);

   util::TextureAtlas& atlas  = util::TextureAtlas::Instance();
   const std::size_t   layers = atlas.LayerCount();
   const std::size_t   width  = atlas.AtlasWidth();
   const std::size_t   height = atlas.AtlasHeight();

   if (layers == 0 || width == 0 || height == 0)
   {
      return result;
   }

   const bool dimsChanged =
      store.atlasTexture_ == nullptr || store.atlasWidth_ != width ||
      store.atlasHeight_ != height || store.atlasLayers_ != layers;
   const bool contentChanged = store.atlasBuildCount_ != buildCount;

   if (dimsChanged || contentChanged)
   {
      if (!OverlayShouldUpload(phase) || commandBuffer == nullptr)
      {
         if (store.atlasTexture_ != nullptr && !dimsChanged)
         {
            result.texture_    = store.atlasTexture_;
            result.sampler_    = store.atlasSampler_;
            result.width_      = store.atlasWidth_;
            result.height_     = store.atlasHeight_;
            result.layers_     = store.atlasLayers_;
            result.buildCount_ = store.atlasBuildCount_;
         }
         return result;
      }

      if (dimsChanged)
      {
         delete store.atlasTexture_;
         store.atlasTexture_ = rhi->newTextureArray(
            QRhiTexture::RGBA8,
            static_cast<int>(layers),
            QSize(static_cast<int>(width), static_cast<int>(height)));
         if (store.atlasTexture_ == nullptr || !store.atlasTexture_->create())
         {
            logger_->error("Failed to create shared texture atlas array");
            delete store.atlasTexture_;
            store.atlasTexture_ = nullptr;
            return result;
         }
         store.atlasWidth_  = width;
         store.atlasHeight_ = height;
         store.atlasLayers_ = layers;
      }

      if (store.atlasSampler_ == nullptr)
      {
         store.atlasSampler_ = rhi->newSampler(QRhiSampler::Linear,
                                               QRhiSampler::Linear,
                                               QRhiSampler::None,
                                               QRhiSampler::ClampToEdge,
                                               QRhiSampler::ClampToEdge,
                                               QRhiSampler::ClampToEdge);
         if (store.atlasSampler_ == nullptr || !store.atlasSampler_->create())
         {
            logger_->error("Failed to create shared atlas sampler");
            delete store.atlasSampler_;
            store.atlasSampler_ = nullptr;
            return result;
         }
      }

      QRhiResourceUpdateBatch* batch =
         AcquireOverlayBatch(rhi, resourceBatch, phase);
      if (batch == nullptr)
      {
         return result;
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
         batch->uploadTexture(
            store.atlasTexture_,
            QRhiTextureUploadDescription(
               QRhiTextureUploadEntry(static_cast<int>(layer), 0, subUpload)));
      }

      SubmitOverlayBatch(commandBuffer, batch, resourceBatch, phase);
      store.atlasBuildCount_ = buildCount;
   }

   result.texture_    = store.atlasTexture_;
   result.sampler_    = store.atlasSampler_;
   result.width_      = store.atlasWidth_;
   result.height_     = store.atlasHeight_;
   result.layers_     = store.atlasLayers_;
   result.buildCount_ = store.atlasBuildCount_;
   return result;
}

QRhiSampler* AcquireNearestSampler(QRhi* rhi)
{
   if (rhi == nullptr)
   {
      return nullptr;
   }

   std::lock_guard lock {storeMutex_};
   RhiStore&       store = GetStore(rhi);
   if (store.nearestSampler_ != nullptr)
   {
      return store.nearestSampler_;
   }

   store.nearestSampler_ = rhi->newSampler(QRhiSampler::Nearest,
                                           QRhiSampler::Nearest,
                                           QRhiSampler::None,
                                           QRhiSampler::ClampToEdge,
                                           QRhiSampler::ClampToEdge);
   if (store.nearestSampler_ == nullptr || !store.nearestSampler_->create())
   {
      delete store.nearestSampler_;
      store.nearestSampler_ = nullptr;
   }
   return store.nearestSampler_;
}

QRhiGraphicsPipeline*
AcquireColoredGeometryPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   if (rhi == nullptr || renderTarget == nullptr)
   {
      return nullptr;
   }

   std::lock_guard   lock {storeMutex_};
   RhiStore&         store  = GetStore(rhi);
   const PipelineKey key    = MakeKey(renderTarget);
   PipelineBucket&   bucket = store.pipelines_[key];
   if (bucket.coloredGeometry_ != nullptr)
   {
      return bucket.coloredGeometry_;
   }
   if (!EnsureLayoutResources(rhi, bucket))
   {
      return nullptr;
   }

   const QShader vertexShader   = LoadShader(":/gl/vulkan/qsb/color.vert.qsb");
   const QShader fragmentShader = LoadShader(":/gl/vulkan/qsb/color.frag.qsb");
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load colored geometry shaders");
      return nullptr;
   }

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      return nullptr;
   }

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{7 * sizeof(float)}});
   inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float3, 0},
       {0, 1, QRhiVertexInputAttribute::Float4, 3 * sizeof(float)}});

   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (!ConfigureCommonPipeline(
          pipeline.get(), renderTarget, bucket.layoutSrbUniform_))
   {
      logger_->error("Failed to create shared colored geometry pipeline");
      return nullptr;
   }

   bucket.coloredGeometry_ = pipeline.release();
   return bucket.coloredGeometry_;
}

QRhiGraphicsPipeline*
AcquireGeoColoredGeometryPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   if (rhi == nullptr || renderTarget == nullptr)
   {
      return nullptr;
   }

   std::lock_guard   lock {storeMutex_};
   RhiStore&         store  = GetStore(rhi);
   const PipelineKey key    = MakeKey(renderTarget);
   PipelineBucket&   bucket = store.pipelines_[key];
   if (bucket.geoColoredGeometry_ != nullptr)
   {
      return bucket.geoColoredGeometry_;
   }
   if (!EnsureLayoutResources(rhi, bucket))
   {
      return nullptr;
   }

   const QShader vertexShader =
      LoadShader(":/gl/vulkan/qsb/geo_color.vert.qsb");
   const QShader fragmentShader =
      LoadShader(":/gl/vulkan/qsb/geo_color.frag.qsb");
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load geo colored shaders");
      return nullptr;
   }

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      return nullptr;
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

   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (!ConfigureCommonPipeline(
          pipeline.get(), renderTarget, bucket.layoutSrbGeoUniform_))
   {
      logger_->error("Failed to create shared geo colored pipeline");
      return nullptr;
   }

   bucket.geoColoredGeometry_ = pipeline.release();
   return bucket.geoColoredGeometry_;
}

QRhiGraphicsPipeline* AcquireRadarPipeline(QRhi*             rhi,
                                           QRhiRenderTarget* renderTarget)
{
   if (rhi == nullptr || renderTarget == nullptr)
   {
      return nullptr;
   }

   std::lock_guard   lock {storeMutex_};
   RhiStore&         store  = GetStore(rhi);
   const PipelineKey key    = MakeKey(renderTarget);
   PipelineBucket&   bucket = store.pipelines_[key];
   if (bucket.radar_ != nullptr)
   {
      return bucket.radar_;
   }
   if (!EnsureLayoutResources(rhi, bucket))
   {
      return nullptr;
   }

   const QShader vertexShader   = LoadShader(":/gl/vulkan/qsb/radar.vert.qsb");
   const QShader fragmentShader = LoadShader(":/gl/vulkan/qsb/radar.frag.qsb");
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load radar shaders");
      return nullptr;
   }

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      return nullptr;
   }

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings(
      {{2 * sizeof(float)}, {sizeof(std::uint32_t)}, {sizeof(std::uint32_t)}});
   inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                              {1, 1, QRhiVertexInputAttribute::UInt, 0},
                              {2, 2, QRhiVertexInputAttribute::UInt, 0}});

   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (!ConfigureCommonPipeline(
          pipeline.get(), renderTarget, bucket.layoutSrbUniformTexture_))
   {
      logger_->error("Failed to create shared radar pipeline");
      return nullptr;
   }

   bucket.radar_ = pipeline.release();
   return bucket.radar_;
}

QRhiGraphicsPipeline* AcquireColorTablePipeline(QRhi*             rhi,
                                                QRhiRenderTarget* renderTarget)
{
   if (rhi == nullptr || renderTarget == nullptr)
   {
      return nullptr;
   }

   std::lock_guard   lock {storeMutex_};
   RhiStore&         store  = GetStore(rhi);
   const PipelineKey key    = MakeKey(renderTarget);
   PipelineBucket&   bucket = store.pipelines_[key];
   if (bucket.colorTable_ != nullptr)
   {
      return bucket.colorTable_;
   }
   if (!EnsureLayoutResources(rhi, bucket))
   {
      return nullptr;
   }

   const QShader vertexShader =
      LoadShader(":/gl/vulkan/qsb/texture1d.vert.qsb");
   const QShader fragmentShader =
      LoadShader(":/gl/vulkan/qsb/texture_lut.frag.qsb");
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load color table shaders");
      return nullptr;
   }

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      return nullptr;
   }

   QRhiVertexInputLayout inputLayout;
   inputLayout.setBindings({{2 * sizeof(float)}, {sizeof(float)}});
   inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                              {1, 1, QRhiVertexInputAttribute::Float, 0}});

   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (!ConfigureCommonPipeline(
          pipeline.get(), renderTarget, bucket.layoutSrbUniformTexture_))
   {
      logger_->error("Failed to create shared color table pipeline");
      return nullptr;
   }

   bucket.colorTable_ = pipeline.release();
   return bucket.colorTable_;
}

QRhiGraphicsPipeline*
AcquireTextureArrayGeoPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   if (rhi == nullptr || renderTarget == nullptr)
   {
      return nullptr;
   }

   std::lock_guard   lock {storeMutex_};
   RhiStore&         store  = GetStore(rhi);
   const PipelineKey key    = MakeKey(renderTarget);
   PipelineBucket&   bucket = store.pipelines_[key];
   if (bucket.textureArrayGeo_ != nullptr)
   {
      return bucket.textureArrayGeo_;
   }
   if (!EnsureLayoutResources(rhi, bucket))
   {
      return nullptr;
   }

   const QShader vertexShader =
      LoadShader(":/gl/vulkan/qsb/geo_texture_array.vert.qsb");
   const QShader fragmentShader =
      LoadShader(":/gl/vulkan/qsb/geo_texture_array.frag.qsb");
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load geo texture array shaders");
      return nullptr;
   }

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      return nullptr;
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

   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (!ConfigureCommonPipeline(
          pipeline.get(), renderTarget, bucket.layoutSrbGeoTexture_))
   {
      logger_->error("Failed to create shared geo texture array pipeline");
      return nullptr;
   }

   bucket.textureArrayGeo_ = pipeline.release();
   return bucket.textureArrayGeo_;
}

QRhiGraphicsPipeline*
AcquireTextureArrayScreenPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
{
   if (rhi == nullptr || renderTarget == nullptr)
   {
      return nullptr;
   }

   std::lock_guard   lock {storeMutex_};
   RhiStore&         store  = GetStore(rhi);
   const PipelineKey key    = MakeKey(renderTarget);
   PipelineBucket&   bucket = store.pipelines_[key];
   if (bucket.textureArrayScreen_ != nullptr)
   {
      return bucket.textureArrayScreen_;
   }
   if (!EnsureLayoutResources(rhi, bucket))
   {
      return nullptr;
   }

   const QShader vertexShader =
      LoadShader(":/gl/vulkan/qsb/screen_texture_array.vert.qsb");
   const QShader fragmentShader =
      LoadShader(":/gl/vulkan/qsb/screen_texture_array.frag.qsb");
   if (!vertexShader.isValid() || !fragmentShader.isValid())
   {
      logger_->error("Failed to load screen texture array shaders");
      return nullptr;
   }

   std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi->newGraphicsPipeline());
   if (pipeline == nullptr)
   {
      return nullptr;
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

   pipeline->setShaderStages(
      {QRhiShaderStage {QRhiShaderStage::Vertex, vertexShader},
       QRhiShaderStage {QRhiShaderStage::Fragment, fragmentShader}});
   pipeline->setVertexInputLayout(inputLayout);
   if (!ConfigureCommonPipeline(
          pipeline.get(), renderTarget, bucket.layoutSrbScreenTexture_))
   {
      logger_->error("Failed to create shared screen texture array pipeline");
      return nullptr;
   }

   bucket.textureArrayScreen_ = pipeline.release();
   return bucket.textureArrayScreen_;
}

void RetainOverlayGpuStore(QRhi* rhi)
{
   if (rhi == nullptr)
   {
      return;
   }

   std::lock_guard lock {storeMutex_};
   ++GetStore(rhi).retainCount_;
}

void ReleaseOverlayGpuStore(QRhi* rhi)
{
   if (rhi == nullptr)
   {
      return;
   }

   std::lock_guard lock {storeMutex_};
   const auto      it = stores_.find(rhi);
   if (it == stores_.end())
   {
      return;
   }

   if (it->second.retainCount_ > 0)
   {
      --it->second.retainCount_;
   }

   if (it->second.retainCount_ == 0)
   {
      it->second.Destroy();
      stores_.erase(it);
   }
}

} // namespace scwx::qt::render
