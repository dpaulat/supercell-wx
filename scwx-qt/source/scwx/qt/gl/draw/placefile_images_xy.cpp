#include <scwx/qt/gl/draw/placefile_images_xy.hpp>
#include <scwx/qt/types/placefile_types.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <QDir>
#include <QUrl>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx::qt::gl::draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_images_xy";
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

class PlacefileImagesXY::Impl
{
public:
   explicit Impl(const std::shared_ptr<GlContext>& context) : context_ {context}
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

   std::shared_ptr<GlContext> context_;

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

   std::shared_ptr<ShaderProgram> shaderProgram_ {nullptr};

   GLint uMVPMatrixLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};

   GLuint                vao_ {GL_INVALID_INDEX};
   std::array<GLuint, 2> vbo_ {GL_INVALID_INDEX};

   GLsizei numVertices_ {0};
};

PlacefileImagesXY::PlacefileImagesXY(
   const std::shared_ptr<GlContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefileImagesXY::~PlacefileImagesXY() = default;

PlacefileImagesXY::PlacefileImagesXY(PlacefileImagesXY&&) noexcept = default;
PlacefileImagesXY&
PlacefileImagesXY::operator=(PlacefileImagesXY&&) noexcept = default;

void PlacefileImagesXY::Initialize()
{
   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/texture2d_array.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/texture2d_array.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");

   glGenVertexArrays(1, &p->vao_);
   glGenBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   glBindVertexArray(p->vao_);
   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // NOLINTBEGIN(modernize-use-nullptr)
   // NOLINTBEGIN(performance-no-int-to-ptr)
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

   // aVertex
   glVertexAttribPointer(0,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         static_cast<void*>(0));
   glEnableVertexAttribArray(0);

   // aAnchor
   glVertexAttribPointer(6,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(2 * sizeof(float)));
   glEnableVertexAttribArray(6);

   // aModulate
   glVertexAttribPointer(3,
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(4 * sizeof(float)));
   glEnableVertexAttribArray(3);

   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aTexCoord
   glVertexAttribPointer(2,
                         3,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerTexCoord * sizeof(float),
                         static_cast<void*>(0));
   glEnableVertexAttribArray(2);

   // aDisplayed
   glVertexAttribI1i(5, 1);

   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
   // NOLINTEND(performance-no-int-to-ptr)
   // NOLINTEND(modernize-use-nullptr)

   p->dirty_ = true;
}

void PlacefileImagesXY::Render(
   const QMapLibre::CustomLayerRenderParameters& params,
   bool                                          textureAtlasChanged)
{
   const std::unique_lock lock {p->imageMutex_};

   if (!p->currentImageList_.empty())
   {
      glBindVertexArray(p->vao_);

      p->Update(textureAtlasChanged);
      p->shaderProgram_->Use();
      UseDefaultProjection(params, p->uMVPMatrixLocation_);

      // Interpolate texture coordinates
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // Draw images
      glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefileImagesXY::Deinitialize()
{
   glDeleteVertexArrays(1, &p->vao_);
   glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

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
   // If the texture atlas has changed
   if (dirty_ || textureAtlasChanged)
   {
      // Update texture coordinates
      for (auto& imageFile : currentImageFiles_)
      {
         imageFile.second.UpdateTextureInfo();
      }

      // Update OpenGL texture buffer data
      UpdateTextureBuffer();

      // Buffer texture data
      glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      glBufferData(
         GL_ARRAY_BUFFER,
         static_cast<GLsizeiptr>(sizeof(float) * textureBuffer_.size()),
         textureBuffer_.data(),
         GL_DYNAMIC_DRAW);
   }

   // If buffers need updating
   if (dirty_)
   {
      // Buffer vertex data
      glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      glBufferData(
         GL_ARRAY_BUFFER,
         static_cast<GLsizeiptr>(sizeof(float) * currentImageBuffer_.size()),
         currentImageBuffer_.data(),
         GL_DYNAMIC_DRAW);

      numVertices_ =
         static_cast<GLsizei>(currentImageBuffer_.size() / kPointsPerVertex);
   }

   dirty_ = false;
}

} // namespace scwx::qt::gl::draw
