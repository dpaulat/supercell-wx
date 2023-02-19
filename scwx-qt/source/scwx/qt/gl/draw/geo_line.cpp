#include <scwx/qt/gl/draw/geo_line.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <numbers>
#include <optional>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::geo_line";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t kNumRectangles        = 1;
static constexpr size_t kNumTriangles         = kNumRectangles * 2;
static constexpr size_t kVerticesPerTriangle  = 3;
static constexpr size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr size_t kPointsPerVertex      = 10;
static constexpr size_t kBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;

class GeoLine::Impl
{
public:
   explicit Impl(std::shared_ptr<GlContext> context) :
       context_ {context},
       geodesic_ {util::GeographicLib::DefaultGeodesic()},
       dirty_ {false},
       visible_ {true},
       points_ {},
       angle_ {},
       width_ {7.0f},
       modulateColor_ {std::nullopt},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       texture_ {},
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX}
   {
   }

   ~Impl() {}

   std::shared_ptr<GlContext> context_;

   const GeographicLib::Geodesic& geodesic_;

   bool dirty_;

   bool                              visible_;
   std::array<common::Coordinate, 2> points_;
   float                             angle_;
   float                             width_;

   std::optional<boost::gil::rgba8_pixel_t> modulateColor_;

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;

   util::TextureAttributes texture_;

   GLuint vao_;
   GLuint vbo_;

   void Update();
};

GeoLine::GeoLine(std::shared_ptr<GlContext> context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
GeoLine::~GeoLine() = default;

GeoLine::GeoLine(GeoLine&&) noexcept            = default;
GeoLine& GeoLine::operator=(GeoLine&&) noexcept = default;

void GeoLine::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(":/gl/geo_line.vert",
                                                     ":/gl/texture2d.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      logger_->warn("Could not find uMVPMatrix");
   }

   p->uMapMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uMapMatrix");
   if (p->uMapMatrixLocation_ == -1)
   {
      logger_->warn("Could not find uMapMatrix");
   }

   p->uMapScreenCoordLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uMapScreenCoord");
   if (p->uMapScreenCoordLocation_ == -1)
   {
      logger_->warn("Could not find uMapScreenCoord");
   }

   p->texture_ =
      util::TextureAtlas::Instance().GetTextureAttributes("lines/default-1x7");

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(1, &p->vbo_);

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   gl.glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * kBufferLength, nullptr, GL_DYNAMIC_DRAW);

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

   p->dirty_ = true;
}

void GeoLine::Render(const QMapLibreGL::CustomLayerRenderParameters& params)
{
   if (p->visible_)
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);
      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);

      p->Update();
      p->shaderProgram_->Use();
      UseDefaultProjection(params, p->uMVPMatrixLocation_);
      UseMapProjection(
         params, p->uMapMatrixLocation_, p->uMapScreenCoordLocation_);

      // Draw line
      gl.glDrawArrays(GL_TRIANGLES, 0, 6);
   }
}

void GeoLine::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(1, &p->vbo_);
}

void GeoLine::SetPoints(float latitude1,
                        float longitude1,
                        float latitude2,
                        float longitude2)
{
   if (p->points_[0].latitude_ != latitude1 ||
       p->points_[0].longitude_ != longitude1 ||
       p->points_[1].latitude_ != latitude2 ||
       p->points_[1].longitude_ != longitude2)
   {
      p->points_[0] = {latitude1, longitude1};
      p->points_[1] = {latitude2, longitude2};

      double azi1; // Azimuth at point 1 (degrees)
      double azi2; // (Forward) azimuth at point 2 (degrees)
      p->geodesic_.Inverse(p->points_[0].latitude_,
                           p->points_[0].longitude_,
                           p->points_[1].latitude_,
                           p->points_[1].longitude_,
                           azi1,
                           azi2);
      p->angle_ = -azi1 * std::numbers::pi / 180.0;

      p->dirty_ = true;
   }
}

void GeoLine::SetModulateColor(boost::gil::rgba8_pixel_t color)
{
   if (p->modulateColor_ != color)
   {
      p->modulateColor_ = color;
      p->dirty_         = true;
   }
}

void GeoLine::SetWidth(float width)
{
   if (p->width_ != width)
   {
      p->width_ = width;
      p->dirty_ = true;
   }
}

void GeoLine::SetVisible(bool visible)
{
   p->visible_ = visible;
}

void GeoLine::Impl::Update()
{
   if (dirty_)
   {
      gl::OpenGLFunctions& gl = context_->gl();

      // Latitude and longitude coordinates in degrees
      const float lx = points_[0].latitude_;
      const float rx = points_[1].latitude_;
      const float by = points_[0].longitude_;
      const float ty = points_[1].longitude_;

      // Offset x/y in pixels
      const float ox = width_ * 0.5f * std::cosf(angle_);
      const float oy = width_ * 0.5f * std::sinf(angle_);

      // Texture coordinates
      const float ls = texture_.sLeft_;
      const float rs = texture_.sRight_;
      const float tt = texture_.tTop_;
      const float bt = texture_.tBottom_;

      float mc0 = 1.0f;
      float mc1 = 1.0f;
      float mc2 = 1.0f;
      float mc3 = 1.0f;

      if (modulateColor_.has_value())
      {
         boost::gil::rgba8_pixel_t& mc = modulateColor_.value();

         mc0 = mc[0] / 255.0f;
         mc1 = mc[1] / 255.0f;
         mc2 = mc[2] / 255.0f;
         mc3 = mc[3] / 255.0f;
      }

      const float buffer[kNumRectangles][kVerticesPerRectangle]
                        [kPointsPerVertex] = //
         {                                   //
          // Line
          {
             {lx, by, -ox, -oy, ls, bt, mc0, mc1, mc2, mc3}, // BL
             {lx, by, +ox, +oy, ls, tt, mc0, mc1, mc2, mc3}, // TL
             {rx, ty, -ox, -oy, rs, bt, mc0, mc1, mc2, mc3}, // BR
             {rx, ty, -ox, -oy, rs, bt, mc0, mc1, mc2, mc3}, // BR
             {rx, ty, +ox, +oy, rs, tt, mc0, mc1, mc2, mc3}, // TR
             {lx, by, +ox, +oy, ls, tt, mc0, mc1, mc2, mc3}  // TL
          }};

      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * kBufferLength,
                      buffer,
                      GL_DYNAMIC_DRAW);

      dirty_ = false;
   }
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
