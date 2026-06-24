#include <scwx/qt/draw/placefile_images_xy.hpp>
#include <scwx/qt/types/placefile_types.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <scwx/qt/render/projection.hpp>
#include <scwx/qt/render/rhi_texture_array_overlay.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QDir>
#include <QUrl>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx::qt::draw
{

static const std::string logPrefix_ = "scwx::qt::draw::placefile_images_xy";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kNumRectangles       = 1;
static constexpr std::size_t kNumTriangles        = kNumRectangles * 2;
static constexpr std::size_t kVerticesPerTriangle = 3;
static constexpr std::size_t kPointsPerVertex     = 8;
static constexpr std::size_t kPointsPerTexCoord   = 3;
static constexpr std::size_t kImageBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;
static constexpr std::size_t kTextureBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerTexCoord;

static constexpr std::size_t kScreenFloatsPerVertex_ = 10;

static std::vector<float>
BuildScreenImageVertices(const std::vector<float>& imageBuffer,
                         const glm::mat4&          projection)
{
   const std::size_t  vertexCount = imageBuffer.size() / kPointsPerVertex;
   std::vector<float> screenVertices(vertexCount * kScreenFloatsPerVertex_);

   for (std::size_t v = 0; v < vertexCount; ++v)
   {
      const std::size_t imageOffset  = v * kPointsPerVertex;
      const std::size_t screenOffset = v * kScreenFloatsPerVertex_;

      const float x  = imageBuffer[imageOffset + 0];
      const float y  = imageBuffer[imageOffset + 1];
      const float ax = imageBuffer[imageOffset + 2];
      const float ay = imageBuffer[imageOffset + 3];

      glm::vec4 position = projection * glm::vec4 {x, y, 0.0f, 1.0f};
      position.x += ax;
      position.y += ay;

      screenVertices[screenOffset + 0] = position.x;
      screenVertices[screenOffset + 1] = position.y;
      screenVertices[screenOffset + 2] = 0.0f;
      screenVertices[screenOffset + 3] = 0.0f;
      screenVertices[screenOffset + 4] = imageBuffer[imageOffset + 4];
      screenVertices[screenOffset + 5] = imageBuffer[imageOffset + 5];
      screenVertices[screenOffset + 6] = imageBuffer[imageOffset + 6];
      screenVertices[screenOffset + 7] = imageBuffer[imageOffset + 7];
      screenVertices[screenOffset + 8] = 0.0f;
      screenVertices[screenOffset + 9] = 1.0f;
   }

   return screenVertices;
}

class PlacefileImagesXY::Impl
{
public:
   explicit Impl(const std::shared_ptr<render::RenderContext>& context) :
       context_ {context}
   {
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void UpdateBuffers();
   void UpdateTextureBuffer();
   void Update(bool textureAtlasChanged);
   void UpdateVulkan(bool textureAtlasChanged);

   std::shared_ptr<render::RenderContext> context_;

   std::string baseUrl_ {};

   bool dirty_ {false};

   std::mutex imageMutex_;

   boost::unordered_flat_map<std::string, types::PlacefileImageInfo>
      currentImageFiles_ {};
   boost::unordered_flat_map<std::string, types::PlacefileImageInfo>
      newImageFiles_ {};

   std::vector<std::shared_ptr<const gr::Placefile::ImageXYDrawItem>>
      currentImageList_ {};
   std::vector<std::shared_ptr<const gr::Placefile::ImageXYDrawItem>>
      newImageList_ {};

   std::vector<float> currentImageBuffer_ {};
   std::vector<float> newImageBuffer_ {};

   std::vector<float> textureBuffer_ {};

   std::uint32_t numVertices_ {0};
};

PlacefileImagesXY::PlacefileImagesXY(
   const std::shared_ptr<render::RenderContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefileImagesXY::~PlacefileImagesXY() = default;

PlacefileImagesXY::PlacefileImagesXY(PlacefileImagesXY&&) noexcept = default;
PlacefileImagesXY&
PlacefileImagesXY::operator=(PlacefileImagesXY&&) noexcept = default;

void PlacefileImagesXY::Initialize() {}

void PlacefileImagesXY::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   bool /* textureAtlasChanged */)
{
}

void PlacefileImagesXY::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool                                          textureAtlasChanged)
{
   const std::unique_lock lock {p->imageMutex_};

   if (p->currentImageList_.empty())
   {
      return;
   }

   p->UpdateVulkan(textureAtlasChanged);

   resources.textureArrayOverlay.SyncAtlas(commandBuffer,
                                           p->context_->texture_buffer_count());

   glm::mat4 projection = scwx::qt::render::OrthoMapProjection(params);
   projection =
      glm::rotate(projection,
                  glm::radians<float>(static_cast<float>(params.bearing)),
                  glm::vec3(0.0f, 0.0f, 1.0f));

   const std::vector<float> screenVertices =
      BuildScreenImageVertices(p->currentImageBuffer_, projection);
   const glm::mat4 identity {1.0f};

   resources.textureArrayOverlay.RenderScreen(
      commandBuffer,
      identity,
      screenVertices,
      p->textureBuffer_,
      static_cast<std::uint32_t>(p->numVertices_));
}

void PlacefileImagesXY::Deinitialize()
{
   const std::unique_lock lock {p->imageMutex_};

   p->currentImageList_.clear();
   p->currentImageFiles_.clear();
   p->currentImageBuffer_.clear();
   p->textureBuffer_.clear();
}

void PlacefileImagesXY::StartImagesXY(const std::string& baseUrl)
{
   p->baseUrl_ = baseUrl;

   // Clear the new buffer
   p->newImageList_.clear();
   p->newImageFiles_.clear();
   p->newImageBuffer_.clear();
}

void PlacefileImagesXY::AddImageXY(
   const std::shared_ptr<gr::Placefile::ImageXYDrawItem>& di)
{
   if (di != nullptr)
   {
      p->newImageList_.emplace_back(di);
   }
}

void PlacefileImagesXY::FinishImagesXY()
{
   // Update buffers
   p->UpdateBuffers();

   const std::unique_lock lock {p->imageMutex_};

   // Swap buffers
   p->currentImageList_.swap(p->newImageList_);
   p->currentImageFiles_.swap(p->newImageFiles_);
   p->currentImageBuffer_.swap(p->newImageBuffer_);

   // Clear the new buffers
   p->newImageList_.clear();
   p->newImageFiles_.clear();
   p->newImageBuffer_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileImagesXY::Impl::UpdateBuffers()
{
   newImageBuffer_.clear();
   newImageBuffer_.reserve(newImageList_.size() * kImageBufferLength);
   newImageFiles_.clear();

   // Fixed modulate color
   static constexpr float mc0 = 1.0f;
   static constexpr float mc1 = 1.0f;
   static constexpr float mc2 = 1.0f;
   static constexpr float mc3 = 1.0f;

   for (auto& di : newImageList_)
   {
      // Populate image file map
      newImageFiles_.emplace(std::piecewise_construct,
                             std::tuple {di->imageFile_},
                             std::forward_as_tuple(types::PlacefileImageInfo {
                                di->imageFile_, baseUrl_}));

      // Limit processing to groups of 3 (triangles)
      const std::size_t numElements =
         di->elements_.size() - di->elements_.size() % 3;
      for (std::size_t i = 0; i < numElements; ++i)
      {
         const auto& element = di->elements_[i];

         // X and Y coordinates in pixels
         const auto x  = static_cast<float>(element.x_);
         const auto y  = static_cast<float>(element.y_);
         const auto ax = static_cast<float>(element.anchorX_ + 1.0);
         const auto ay = static_cast<float>(element.anchorY_ + 1.0);

         newImageBuffer_.insert(newImageBuffer_.end(),
                                {x, y, ax, ay, mc0, mc1, mc2, mc3});
      }
   }
}

void PlacefileImagesXY::Impl::UpdateTextureBuffer()
{
   textureBuffer_.clear();
   textureBuffer_.reserve(currentImageList_.size() * kTextureBufferLength);

   for (const auto& di : currentImageList_)
   {
      // Get placefile image info. The key should always be found in the map, as
      // it is populated when the placefile is updated.
      const auto it = currentImageFiles_.find(di->imageFile_);
      const types::PlacefileImageInfo& image =
         (it == currentImageFiles_.cend()) ?
            currentImageFiles_.cbegin()->second :
            it->second;

      const auto r = static_cast<float>(image.texture_.layerId_);

      // Limit processing to groups of 3 (triangles)
      const std::size_t numElements =
         di->elements_.size() - di->elements_.size() % 3;
      for (std::size_t i = 0; i < numElements; ++i)
      {
         const auto& element = di->elements_[i];

         // Texture coordinates
         const auto s = static_cast<float>(image.texture_.sLeft_ +
                                           (image.scaledWidth_ * element.tu_));
         const auto t = static_cast<float>(image.texture_.tTop_ +
                                           (image.scaledHeight_ * element.tv_));

         textureBuffer_.insert(textureBuffer_.end(), {s, t, r});
      }
   }
}

void PlacefileImagesXY::Impl::Update(bool textureAtlasChanged)
{
   if (dirty_ || textureAtlasChanged)
   {
      for (auto& imageFile : currentImageFiles_)
      {
         imageFile.second.UpdateTextureInfo();
      }

      UpdateTextureBuffer();
   }

   if (dirty_)
   {
      numVertices_ = static_cast<std::uint32_t>(currentImageBuffer_.size() /
                                                kPointsPerVertex);
   }

   dirty_ = false;
}

void PlacefileImagesXY::Impl::UpdateVulkan(bool textureAtlasChanged)
{
   if (dirty_ || textureAtlasChanged)
   {
      for (auto& imageFile : currentImageFiles_)
      {
         imageFile.second.UpdateTextureInfo();
      }

      UpdateTextureBuffer();
   }

   if (dirty_)
   {
      numVertices_ = static_cast<std::uint32_t>(currentImageBuffer_.size() /
                                                kPointsPerVertex);
   }

   dirty_ = false;
}

} // namespace scwx::qt::draw
