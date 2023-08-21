#include <scwx/qt/gl/draw/placefile_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
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

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_lines";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kNumRectangles        = 1;
static constexpr std::size_t kNumTriangles         = kNumRectangles * 2;
static constexpr std::size_t kVerticesPerTriangle  = 3;
static constexpr std::size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr std::size_t kPointsPerVertex      = 11;
static constexpr std::size_t kBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;

static const std::string kTextureName_ = "lines/default-1x7";

class PlacefileLines::Impl
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

   std::mutex lineMutex_;

   std::vector<std::shared_ptr<const gr::Placefile::LineDrawItem>>
      currentLineList_ {};
   std::vector<std::shared_ptr<const gr::Placefile::LineDrawItem>>
      newLineList_ {};

   std::size_t currentNumLines_ {};
   std::size_t newNumLines_ {};

   std::vector<float> lineBuffer_ {};
   std::vector<GLint> thresholdBuffer_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;

   void UpdateBuffers();
   void Update(bool textureAtlasChanged);
};

PlacefileLines::PlacefileLines(std::shared_ptr<GlContext> context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
PlacefileLines::~PlacefileLines() = default;

PlacefileLines::PlacefileLines(PlacefileLines&&) noexcept            = default;
PlacefileLines& PlacefileLines::operator=(PlacefileLines&&) noexcept = default;

void PlacefileLines::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileLines::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/geo_texture2d.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/color.frag"}});

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
   gl.glVertexAttribDivisor(5, 2); // One value per rectangle
   gl.glEnableVertexAttribArray(5);

   p->dirty_ = true;
}

void PlacefileLines::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   bool                                            textureAtlasChanged)
{
   std::unique_lock lock {p->lineMutex_};

   if (!p->currentLineList_.empty())
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
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefileLines::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(2, p->vbo_.data());

   std::unique_lock lock {p->lineMutex_};

   p->currentLineList_.clear();
   p->lineBuffer_.clear();
   p->thresholdBuffer_.clear();
}

void PlacefileLines::StartLines()
{
   // Clear the new buffer
   p->newLineList_.clear();

   p->newNumLines_ = 0u;
}

void PlacefileLines::AddLine(
   const std::shared_ptr<gr::Placefile::LineDrawItem>& di)
{
   if (di != nullptr)
   {
      p->newLineList_.emplace_back(di);
      if (!di->elements_.empty())
      {
         p->newNumLines_ += di->elements_.size() - 1;
      }
   }
}

void PlacefileLines::FinishLines()
{
   std::unique_lock lock {p->lineMutex_};

   // Swap buffers
   p->currentLineList_.swap(p->newLineList_);

   // Clear the new buffers
   p->newLineList_.clear();

   // Update the number of lines
   p->currentNumLines_ = p->newNumLines_;

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileLines::Impl::UpdateBuffers()
{
   auto texture =
      util::TextureAtlas::Instance().GetTextureAttributes(kTextureName_);

   // Texture coordinates (rotated)
   const float ls = texture.tTop_;
   const float rs = texture.tBottom_;
   const float tt = texture.sLeft_;
   const float bt = texture.sRight_;

   lineBuffer_.clear();
   lineBuffer_.reserve(currentNumLines_ * kBufferLength);
   thresholdBuffer_.clear();
   thresholdBuffer_.reserve(currentNumLines_);
   numVertices_ = 0;

   for (auto& di : currentLineList_)
   {
      // Threshold value
      units::length::nautical_miles<double> threshold = di->threshold_;
      GLint thresholdValue = static_cast<GLint>(std::round(threshold.value()));

      // TODO: Angle in degrees
      // const float a = 0.0f;

      // Line width / half width
      const float lw = static_cast<float>(di->width_);
      const float hw = lw * 0.5f;

      // For each element pair inside a Line statement
      for (std::size_t i = 0; i < di->elements_.size() - 1; ++i)
      {
         // Latitude and longitude coordinates in degrees
         const float lat1 = static_cast<float>(di->elements_[i].latitude_);
         const float lon1 = static_cast<float>(di->elements_[i].longitude_);
         const float lat2 = static_cast<float>(di->elements_[i + 1].latitude_);
         const float lon2 = static_cast<float>(di->elements_[i + 1].longitude_);

         // TODO: Base X/Y offsets in pixels
         // const float x1 = static_cast<float>(di->elements_[i].x_);
         // const float y1 = static_cast<float>(di->elements_[i].y_);
         // const float x2 = static_cast<float>(di->elements_[i + 1].x_);
         // const float y2 = static_cast<float>(di->elements_[i + 1].y_);

         // TODO: Refactor this to placefile update time instead of buffer time
         const units::angle::degrees<double> angle =
            util::GeographicLib::GetAngle(lat1, lon1, lat2, lon2);
         const float a = static_cast<float>(angle.value());

         // Final X/Y offsets in pixels
         const float lx = -hw;
         const float rx = +hw;
         const float ty = +hw;
         const float by = -hw;

         // Modulate color
         const float mc0 = di->color_[0] / 255.0f;
         const float mc1 = di->color_[1] / 255.0f;
         const float mc2 = di->color_[2] / 255.0f;
         const float mc3 = di->color_[3] / 255.0f;

         lineBuffer_.insert(
            lineBuffer_.end(),
            {
               // Icon
               lat1, lon1, lx, by, ls, bt, mc0, mc1, mc2, mc3, a, // BL
               lat2, lon2, lx, ty, ls, tt, mc0, mc1, mc2, mc3, a, // TL
               lat1, lon1, rx, by, rs, bt, mc0, mc1, mc2, mc3, a, // BR
               lat1, lon1, rx, by, rs, bt, mc0, mc1, mc2, mc3, a, // BR
               lat2, lon2, rx, ty, rs, tt, mc0, mc1, mc2, mc3, a, // TR
               lat2, lon2, lx, ty, ls, tt, mc0, mc1, mc2, mc3, a  // TL
            });
         thresholdBuffer_.push_back(thresholdValue);
      }
   }

   dirty_ = true;
}

void PlacefileLines::Impl::Update(bool textureAtlasChanged)
{
   // If the texture atlas has changed
   if (dirty_ || textureAtlasChanged)
   {
      // Update OpenGL buffer data
      UpdateBuffers();

      gl::OpenGLFunctions& gl = context_->gl();

      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * lineBuffer_.size(),
                      lineBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * thresholdBuffer_.size(),
                      thresholdBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      numVertices_ =
         static_cast<GLsizei>(lineBuffer_.size() / kVerticesPerRectangle);
   }

   dirty_ = false;
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
