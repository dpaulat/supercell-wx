#include <scwx/qt/gl/draw/placefile_icons.hpp>
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

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_icons";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kNumRectangles        = 1;
static constexpr std::size_t kNumTriangles         = kNumRectangles * 2;
static constexpr std::size_t kVerticesPerTriangle  = 3;
static constexpr std::size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr std::size_t kPointsPerVertex      = 11;
static constexpr std::size_t kBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;

struct PlacefileIconInfo
{
   PlacefileIconInfo(
      const std::shared_ptr<const gr::Placefile::IconFile>& iconFile,
      const std::string&                                    baseUrlString) :
       iconFile_ {iconFile}
   {
      // Resolve using base URL
      auto baseUrl = QUrl::fromUserInput(QString::fromStdString(baseUrlString));
      auto relativeUrl = QUrl(QString::fromStdString(iconFile->filename_));

      texture_ = util::TextureAtlas::Instance().GetTextureAttributes(
         baseUrl.resolved(relativeUrl).toString().toStdString());

      if (iconFile->iconWidth_ > 0 && iconFile->iconHeight_ > 0)
      {
         columns_ = texture_.size_.x / iconFile->iconWidth_;
         rows_    = texture_.size_.y / iconFile->iconHeight_;
      }
      else
      {
         columns_ = 0u;
         rows_    = 0u;
      }

      numIcons_ = columns_ * rows_;

      // Pixel size
      float xFactor = 0.0f;
      float yFactor = 0.0f;

      if (texture_.size_.x > 0 && texture_.size_.y > 0)
      {
         xFactor = (texture_.sRight_ - texture_.sLeft_) / texture_.size_.x;
         yFactor = (texture_.tBottom_ - texture_.tTop_) / texture_.size_.y;
      }

      scaledWidth_  = iconFile_->iconWidth_ * xFactor;
      scaledHeight_ = iconFile_->iconHeight_ * yFactor;
   }

   std::shared_ptr<const gr::Placefile::IconFile> iconFile_;
   util::TextureAttributes                        texture_;
   std::size_t                                    rows_ {};
   std::size_t                                    columns_ {};
   std::size_t                                    numIcons_ {};
   float                                          scaledWidth_ {};
   float                                          scaledHeight_ {};
};

class PlacefileIcons::Impl
{
public:
   explicit Impl(std::shared_ptr<GlContext> context) :
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

   std::shared_ptr<GlContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   boost::unordered_flat_map<std::size_t, const PlacefileIconInfo>
      iconFiles_ {};

   std::vector<std::shared_ptr<const gr::Placefile::IconDrawItem>> iconList_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;

   void Update();
};

PlacefileIcons::PlacefileIcons(std::shared_ptr<GlContext> context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
PlacefileIcons::~PlacefileIcons() = default;

PlacefileIcons::PlacefileIcons(PlacefileIcons&&) noexcept            = default;
PlacefileIcons& PlacefileIcons::operator=(PlacefileIcons&&) noexcept = default;

void PlacefileIcons::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileIcons::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/geo_texture2d.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/texture2d.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");
   p->uMapMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMapMatrix");
   p->uMapScreenCoordLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapScreenCoord");
   p->uMapDistanceLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapDistance");

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(2, p->vbo_.data());

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

   // aTexCoord
   gl.glVertexAttribPointer(2,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(2);

   // aModulate
   gl.glVertexAttribPointer(3,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(6 * sizeof(float)));
   gl.glEnableVertexAttribArray(3);

   // aAngle
   gl.glVertexAttribPointer(4,
                            1,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(10 * sizeof(float)));
   gl.glEnableVertexAttribArray(4);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
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

void PlacefileIcons::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   if (!p->iconList_.empty())
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);

      p->Update();
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

      // Don't interpolate texture coordinates
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefileIcons::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(2, p->vbo_.data());
}

void PlacefileIcons::SetIconFiles(
   const std::vector<std::shared_ptr<const gr::Placefile::IconFile>>& iconFiles,
   const std::string&                                                 baseUrl)
{
   p->dirty_ = true;

   // Populate icon file map
   p->iconFiles_.clear();

   for (auto& file : iconFiles)
   {
      p->iconFiles_.emplace(
         std::piecewise_construct,
         std::tuple {file->fileNumber_},
         std::forward_as_tuple(PlacefileIconInfo {file, baseUrl}));
   }
}

void PlacefileIcons::AddIcon(
   const std::shared_ptr<gr::Placefile::IconDrawItem>& di)
{
   if (di != nullptr)
   {
      p->iconList_.emplace_back(di);
      p->dirty_ = true;
   }
}

void PlacefileIcons::Reset()
{
   // Clear the icon list, and mark the draw item dirty
   p->iconList_.clear();
   p->dirty_ = true;
}

void PlacefileIcons::Impl::Update()
{
   if (dirty_)
   {
      static std::vector<float> buffer {};
      static std::vector<GLint> thresholds {};
      buffer.clear();
      buffer.reserve(iconList_.size() * kBufferLength);
      thresholds.clear();
      thresholds.reserve(iconList_.size() * kVerticesPerRectangle);
      numVertices_ = 0;

      for (auto& di : iconList_)
      {
         auto it = iconFiles_.find(di->fileNumber_);
         if (it == iconFiles_.cend())
         {
            // No file found
            logger_->trace("Could not find file number: {}", di->fileNumber_);
            continue;
         }

         auto& icon = it->second;

         // Validate icon
         if (di->iconNumber_ == 0 || di->iconNumber_ > icon.numIcons_)
         {
            // No icon found
            logger_->trace("Invalid icon number: {}", di->iconNumber_);
            continue;
         }

         // Threshold value
         units::length::nautical_miles<double> threshold = di->threshold_;
         GLint                                 thresholdValue =
            static_cast<GLint>(std::round(threshold.value()));

         // Latitude and longitude coordinates in degrees
         const float lat = static_cast<float>(di->latitude_);
         const float lon = static_cast<float>(di->longitude_);

         // Base X/Y offsets in pixels
         const float x = static_cast<float>(di->x_);
         const float y = static_cast<float>(di->y_);

         // Icon size
         const float iw = static_cast<float>(icon.iconFile_->iconWidth_);
         const float ih = static_cast<float>(icon.iconFile_->iconHeight_);

         // Hot X/Y (zero-based icon center)
         const float hx = static_cast<float>(icon.iconFile_->hotX_);
         const float hy = static_cast<float>(icon.iconFile_->hotY_);

         // Final X/Y offsets in pixels
         const float lx = std::roundf(x - hx);
         const float rx = std::roundf(lx + iw);
         const float ty = std::roundf(y + hy);
         const float by = std::roundf(ty - ih);

         // Angle in degrees
         units::angle::degrees<float> angle = di->angle_;
         const float                  a     = angle.value();

         // Texture coordinates
         const std::size_t iconRow    = (di->iconNumber_ - 1) / icon.columns_;
         const std::size_t iconColumn = (di->iconNumber_ - 1) % icon.columns_;

         const float iconX = iconColumn * icon.scaledWidth_;
         const float iconY = iconRow * icon.scaledHeight_;

         const float ls = icon.texture_.sLeft_ + iconX;
         const float rs = ls + icon.scaledWidth_;
         const float tt = icon.texture_.tTop_ + iconY;
         const float bt = tt + icon.scaledHeight_;

         // Fixed modulate color
         const float mc0 = 1.0f;
         const float mc1 = 1.0f;
         const float mc2 = 1.0f;
         const float mc3 = 1.0f;

         buffer.insert(buffer.end(),
                       {
                          // Icon
                          lat, lon, lx, by, ls, bt, mc0, mc1, mc2, mc3, a, // BL
                          lat, lon, lx, ty, ls, tt, mc0, mc1, mc2, mc3, a, // TL
                          lat, lon, rx, by, rs, bt, mc0, mc1, mc2, mc3, a, // BR
                          lat, lon, rx, by, rs, bt, mc0, mc1, mc2, mc3, a, // BR
                          lat, lon, rx, ty, rs, tt, mc0, mc1, mc2, mc3, a, // TR
                          lat, lon, lx, ty, ls, tt, mc0, mc1, mc2, mc3, a  // TL
                       });
         thresholds.insert(thresholds.end(),
                           {thresholdValue, //
                            thresholdValue,
                            thresholdValue,
                            thresholdValue,
                            thresholdValue,
                            thresholdValue});

         numVertices_ += 6;
      }

      gl::OpenGLFunctions& gl = context_->gl();

      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * buffer.size(),
                      buffer.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * thresholds.size(),
                      thresholds.data(),
                      GL_DYNAMIC_DRAW);

      dirty_ = false;
   }
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
