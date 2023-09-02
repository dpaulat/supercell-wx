#include <scwx/qt/gl/draw/placefile_images.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <QUrl>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_images";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kNumRectangles        = 1;
static constexpr std::size_t kNumTriangles         = kNumRectangles * 2;
static constexpr std::size_t kVerticesPerTriangle  = 3;
static constexpr std::size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr std::size_t kPointsPerVertex      = 8;
static constexpr std::size_t kPointsPerTexCoord    = 2;
static constexpr std::size_t kImageBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;
static constexpr std::size_t kTextureBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerTexCoord;

struct PlacefileImageInfo
{
   PlacefileImageInfo(const std::string& imageFile,
                      const std::string& baseUrlString)
   {
      // Resolve using base URL
      auto baseUrl = QUrl::fromUserInput(QString::fromStdString(baseUrlString));
      auto relativeUrl = QUrl(QString::fromStdString(imageFile));
      resolvedUrl_     = baseUrl.resolved(relativeUrl).toString().toStdString();
   }

   void UpdateTextureInfo();

   std::string             resolvedUrl_;
   util::TextureAttributes texture_ {};
   float                   scaledWidth_ {};
   float                   scaledHeight_ {};
};

class PlacefileImages::Impl
{
public:
   explicit Impl(const std::shared_ptr<GlContext>& context) :
       context_ {context},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       uMapDistanceLocation_(GL_INVALID_INDEX),
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
   }

   ~Impl() {}

   void UpdateBuffers();
   void UpdateTextureBuffer();
   void Update(bool textureAtlasChanged);

   std::shared_ptr<GlContext> context_;

   std::string baseUrl_ {};

   bool dirty_ {false};
   bool thresholded_ {false};

   std::mutex imageMutex_;

   boost::unordered_flat_map<std::string, PlacefileImageInfo>
      currentImageFiles_ {};
   boost::unordered_flat_map<std::string, PlacefileImageInfo> newImageFiles_ {};

   std::vector<std::shared_ptr<const gr::Placefile::ImageDrawItem>>
      currentImageList_ {};
   std::vector<std::shared_ptr<const gr::Placefile::ImageDrawItem>>
      newImageList_ {};

   std::vector<float> currentImageBuffer_ {};
   std::vector<GLint> currentThresholdBuffer_ {};
   std::vector<float> newImageBuffer_ {};
   std::vector<GLint> newThresholdBuffer_ {};

   std::vector<float> textureBuffer_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;

   GLuint                vao_;
   std::array<GLuint, 3> vbo_;

   GLsizei numVertices_;
};

PlacefileImages::PlacefileImages(const std::shared_ptr<GlContext>& context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
PlacefileImages::~PlacefileImages() = default;

PlacefileImages::PlacefileImages(PlacefileImages&&) noexcept = default;
PlacefileImages&
PlacefileImages::operator=(PlacefileImages&&) noexcept = default;

void PlacefileImages::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileImages::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/geo_texture2d.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/texture2d_array.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");
   p->uMapMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMapMatrix");
   p->uMapScreenCoordLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapScreenCoord");
   p->uMapDistanceLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapDistance");

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aLatLong
   gl.glVertexAttribPointer(0,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // aXYOffset
   gl.glVertexAttribPointer(1,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(2 * sizeof(float)));
   gl.glEnableVertexAttribArray(1);

   // aModulate
   gl.glVertexAttribPointer(3,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(3);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aTexCoord
   gl.glVertexAttribPointer(2,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerTexCoord * sizeof(float),
                            static_cast<void*>(0));
   gl.glEnableVertexAttribArray(2);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[2]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aThreshold
   gl.glVertexAttribIPointer(5, //
                             1,
                             GL_INT,
                             0,
                             static_cast<void*>(0));
   gl.glEnableVertexAttribArray(5);

   p->dirty_ = true;
}

void PlacefileImages::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   bool                                            textureAtlasChanged)
{
   std::unique_lock lock {p->imageMutex_};

   if (!p->currentImageList_.empty())
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);

      p->Update(textureAtlasChanged);
      p->shaderProgram_->Use();
      UseRotationProjection(params, p->uMVPMatrixLocation_);
      UseMapProjection(
         params, p->uMapMatrixLocation_, p->uMapScreenCoordLocation_);

      if (p->thresholded_)
      {
         // If thresholding is enabled, set the map distance
         units::length::nautical_miles<float> mapDistance =
            util::maplibre::GetMapDistance(params);
         gl.glUniform1f(p->uMapDistanceLocation_, mapDistance.value());
      }
      else
      {
         // If thresholding is disabled, set the map distance to 0
         gl.glUniform1f(p->uMapDistanceLocation_, 0.0f);
      }

      // Interpolate texture coordinates
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // Draw images
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefileImages::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   std::unique_lock lock {p->imageMutex_};

   p->currentImageList_.clear();
   p->currentImageFiles_.clear();
   p->currentImageBuffer_.clear();
   p->currentThresholdBuffer_.clear();
   p->textureBuffer_.clear();
}

void PlacefileImageInfo::UpdateTextureInfo()
{
   texture_ = util::TextureAtlas::Instance().GetTextureAttributes(resolvedUrl_);

   scaledWidth_  = texture_.sRight_ - texture_.sLeft_;
   scaledHeight_ = texture_.tBottom_ - texture_.tTop_;
}

void PlacefileImages::StartImages(const std::string& baseUrl)
{
   p->baseUrl_ = baseUrl;

   // Clear the new buffer
   p->newImageList_.clear();
   p->newImageFiles_.clear();
   p->newImageBuffer_.clear();
   p->newThresholdBuffer_.clear();
}

void PlacefileImages::AddImage(
   const std::shared_ptr<gr::Placefile::ImageDrawItem>& di)
{
   if (di != nullptr)
   {
      p->newImageList_.emplace_back(di);
   }
}

void PlacefileImages::FinishImages()
{
   // Update buffers
   p->UpdateBuffers();

   std::unique_lock lock {p->imageMutex_};

   // Swap buffers
   p->currentImageList_.swap(p->newImageList_);
   p->currentImageFiles_.swap(p->newImageFiles_);
   p->currentImageBuffer_.swap(p->newImageBuffer_);
   p->currentThresholdBuffer_.swap(p->newThresholdBuffer_);

   // Clear the new buffers
   p->newImageList_.clear();
   p->newImageFiles_.clear();
   p->newImageBuffer_.clear();
   p->newThresholdBuffer_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileImages::Impl::UpdateBuffers()
{
   newImageBuffer_.clear();
   newImageBuffer_.reserve(newImageList_.size() * kImageBufferLength);
   newThresholdBuffer_.clear();
   newThresholdBuffer_.reserve(newImageList_.size() * kVerticesPerRectangle);
   newImageFiles_.clear();

   // Fixed modulate color
   static const float mc0 = 1.0f;
   static const float mc1 = 1.0f;
   static const float mc2 = 1.0f;
   static const float mc3 = 1.0f;

   for (auto& di : newImageList_)
   {
      // Populate image file map
      newImageFiles_.emplace(
         std::piecewise_construct,
         std::tuple {di->imageFile_},
         std::forward_as_tuple(PlacefileImageInfo {di->imageFile_, baseUrl_}));

      // Threshold value
      units::length::nautical_miles<double> threshold = di->threshold_;
      GLint thresholdValue = static_cast<GLint>(std::round(threshold.value()));

      // Limit processing to groups of 3 (triangles)
      std::size_t numElements = di->elements_.size() - di->elements_.size() % 3;
      for (std::size_t i = 0; i < numElements; ++i)
      {
         auto& element = di->elements_[i];

         // Latitude and longitude coordinates in degrees
         const float lat = static_cast<float>(element.latitude_);
         const float lon = static_cast<float>(element.longitude_);

         // Base X/Y offsets in pixels
         const float x = static_cast<float>(element.x_);
         const float y = static_cast<float>(element.y_);

         newImageBuffer_.insert(newImageBuffer_.end(),
                                {lat, lon, x, y, mc0, mc1, mc2, mc3});
         newThresholdBuffer_.insert(newThresholdBuffer_.end(),
                                    {thresholdValue});
      }
   }
}

void PlacefileImages::Impl::UpdateTextureBuffer()
{
   textureBuffer_.clear();
   textureBuffer_.reserve(currentImageList_.size() * kTextureBufferLength);

   for (auto& di : currentImageList_)
   {
      // Get placefile image info. The key should always be found in the map, as
      // it is populated when the placefile is updated.
      auto                      it    = currentImageFiles_.find(di->imageFile_);
      const PlacefileImageInfo& image = (it == currentImageFiles_.cend()) ?
                                           currentImageFiles_.cbegin()->second :
                                           it->second;

      // Limit processing to groups of 3 (triangles)
      std::size_t numElements = di->elements_.size() - di->elements_.size() % 3;
      for (std::size_t i = 0; i < numElements; ++i)
      {
         auto& element = di->elements_[i];

         // Texture coordinates
         const float s =
            image.texture_.sLeft_ + (image.scaledWidth_ * element.tu_);
         const float t =
            image.texture_.tTop_ + (image.scaledHeight_ * element.tv_);

         textureBuffer_.insert(textureBuffer_.end(), {s, t});
      }
   }
}

void PlacefileImages::Impl::Update(bool textureAtlasChanged)
{
   gl::OpenGLFunctions& gl = context_->gl();

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
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * textureBuffer_.size(),
                      textureBuffer_.data(),
                      GL_DYNAMIC_DRAW);
   }

   // If buffers need updating
   if (dirty_)
   {
      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * currentImageBuffer_.size(),
                      currentImageBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[2]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * currentThresholdBuffer_.size(),
                      currentThresholdBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      numVertices_ = static_cast<GLsizei>(currentImageBuffer_.size() /
                                          kVerticesPerRectangle);
   }

   dirty_ = false;
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
