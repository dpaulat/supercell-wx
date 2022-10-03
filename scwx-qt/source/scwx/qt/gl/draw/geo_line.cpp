#include <scwx/qt/gl/draw/geo_line.hpp>
#include <scwx/common/geographic.hpp>

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
   explicit Impl(OpenGLFunctions& gl) :
       gl_ {gl},
       dirty_ {false},
       visible_ {true},
       points_ {},
       width_ {1.0f},
       modulateColor_ {std::nullopt},
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX}
   {
   }

   ~Impl() {}

   OpenGLFunctions& gl_;

   bool dirty_;

   bool                              visible_;
   std::array<common::Coordinate, 2> points_;
   float                             width_;

   std::optional<boost::gil::rgba8_pixel_t> modulateColor_;

   // TODO: Texture

   GLuint vao_;
   GLuint vbo_;

   void Update();
};

// TODO: OpenGL context with shaders
GeoLine::GeoLine(OpenGLFunctions& gl) :
    DrawItem(gl), p(std::make_unique<Impl>(gl))
{
}
GeoLine::~GeoLine() = default;

GeoLine::GeoLine(GeoLine&&) noexcept            = default;
GeoLine& GeoLine::operator=(GeoLine&&) noexcept = default;

void GeoLine::Initialize()
{
   gl::OpenGLFunctions& gl = p->gl_;

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

void GeoLine::Render(const QMapbox::CustomLayerRenderParameters&)
{
   if (p->visible_)
   {
      gl::OpenGLFunctions& gl = p->gl_;

      gl.glBindVertexArray(p->vao_);
      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);

      p->Update();

      // Draw line
      gl.glDrawArrays(GL_TRIANGLES, 0, 6);
   }
}

void GeoLine::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->gl_;

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
      p->dirty_     = true;
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
      gl::OpenGLFunctions& gl = gl_;

      const float lx = points_[0].latitude_;
      const float rx = points_[1].latitude_;
      const float by = points_[0].longitude_;
      const float ty = points_[1].longitude_;

      const double i     = points_[1].longitude_ - points_[0].longitude_;
      const double j     = points_[1].latitude_ - points_[0].latitude_;
      const double angle = std::atan2(i, j) * 180.0 / M_PI;
      const float  ox    = width_ * 0.5f * std::cosf(angle);
      const float  oy    = width_ * 0.5f * std::sinf(angle);

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
             {lx, by, -ox, -oy, 0.0f, 0.0f, mc0, mc1, mc2, mc3}, // BL
             {lx, by, +ox, +oy, 0.0f, 1.0f, mc0, mc1, mc2, mc3}, // TL
             {rx, ty, -ox, -oy, 1.0f, 0.0f, mc0, mc1, mc2, mc3}, // BR
             {rx, ty, -ox, -oy, 1.0f, 0.0f, mc0, mc1, mc2, mc3}, // BR
             {rx, ty, +ox, +oy, 1.0f, 1.0f, mc0, mc1, mc2, mc3}, // TR
             {lx, by, +ox, +oy, 0.0f, 1.0f, mc0, mc1, mc2, mc3}  // TL
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
