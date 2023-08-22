#include <scwx/qt/gl/draw/placefile_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

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
static constexpr std::size_t kPointsPerVertex      = 9;
static constexpr std::size_t kBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;

static const boost::gil::rgba8_pixel_t kBlack_ {0, 0, 0, 255};

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

   void BufferLine(const gr::Placefile::LineDrawItem::Element& e1,
                   const gr::Placefile::LineDrawItem::Element& e2,
                   const float                                 width,
                   const float                                 angle,
                   const boost::gil::rgba8_pixel_t             color,
                   const GLint                                 threshold);
   void UpdateBuffers(std::shared_ptr<const gr::Placefile::LineDrawItem>);
   void Update();

   std::shared_ptr<GlContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   std::mutex lineMutex_ {};

   std::size_t currentNumLines_ {};
   std::size_t newNumLines_ {};

   std::vector<float> currentLinesBuffer_ {};
   std::vector<GLint> currentThresholdBuffer_ {};
   std::vector<float> newLinesBuffer_ {};
   std::vector<GLint> newThresholdBuffer_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;
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

   // aModulate
   gl.glVertexAttribPointer(3,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(3);

   // aAngle
   gl.glVertexAttribPointer(4,
                            1,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(8 * sizeof(float)));
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

void PlacefileLines::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   std::unique_lock lock {p->lineMutex_};

   if (p->currentNumLines_ > 0)
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

   p->currentLinesBuffer_.clear();
   p->currentThresholdBuffer_.clear();
}

void PlacefileLines::StartLines()
{
   // Clear the new buffers
   p->newLinesBuffer_.clear();
   p->newThresholdBuffer_.clear();

   p->newNumLines_ = 0u;
}

void PlacefileLines::AddLine(
   const std::shared_ptr<gr::Placefile::LineDrawItem>& di)
{
   if (di != nullptr && !di->elements_.empty())
   {
      p->UpdateBuffers(di);
      p->newNumLines_ += (di->elements_.size() - 1) * 2;
   }
}

void PlacefileLines::FinishLines()
{
   std::unique_lock lock {p->lineMutex_};

   // Swap buffers
   p->currentLinesBuffer_.swap(p->newLinesBuffer_);
   p->currentThresholdBuffer_.swap(p->newThresholdBuffer_);

   // Clear the new buffers
   p->newLinesBuffer_.clear();
   p->newThresholdBuffer_.clear();

   // Update the number of lines
   p->currentNumLines_ = p->newNumLines_;
   p->numVertices_ =
      static_cast<GLsizei>(p->currentNumLines_ * kVerticesPerRectangle);

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileLines::Impl::UpdateBuffers(
   std::shared_ptr<const gr::Placefile::LineDrawItem> di)
{
   // Threshold value
   units::length::nautical_miles<double> threshold = di->threshold_;
   GLint thresholdValue = static_cast<GLint>(std::round(threshold.value()));

   std::vector<float> angles {};
   angles.reserve(di->elements_.size() - 1);

   // For each element pair inside a Line statement, render a black line
   for (std::size_t i = 0; i < di->elements_.size() - 1; ++i)
   {
      // Latitude and longitude coordinates in degrees
      const float lat1 = static_cast<float>(di->elements_[i].latitude_);
      const float lon1 = static_cast<float>(di->elements_[i].longitude_);
      const float lat2 = static_cast<float>(di->elements_[i + 1].latitude_);
      const float lon2 = static_cast<float>(di->elements_[i + 1].longitude_);

      // Calculate angle
      const units::angle::degrees<double> angle =
         util::GeographicLib::GetAngle(lat1, lon1, lat2, lon2);
      float angleValue = angle.value();
      angles.push_back(angleValue);

      BufferLine(di->elements_[i],
                 di->elements_[i + 1],
                 di->width_ + 2,
                 static_cast<float>(angleValue),
                 kBlack_,
                 thresholdValue);
   }

   // For each element pair inside a Line statement, render a colored line
   for (std::size_t i = 0; i < di->elements_.size() - 1; ++i)
   {
      float angleValue = angles[i];

      BufferLine(di->elements_[i],
                 di->elements_[i + 1],
                 di->width_,
                 static_cast<float>(angleValue),
                 di->color_,
                 thresholdValue);
   }
}

void PlacefileLines::Impl::BufferLine(
   const gr::Placefile::LineDrawItem::Element& e1,
   const gr::Placefile::LineDrawItem::Element& e2,
   const float                                 width,
   const float                                 angle,
   const boost::gil::rgba8_pixel_t             color,
   const GLint                                 threshold)
{
   // Latitude and longitude coordinates in degrees
   const float lat1 = static_cast<float>(e1.latitude_);
   const float lon1 = static_cast<float>(e1.longitude_);
   const float lat2 = static_cast<float>(e2.latitude_);
   const float lon2 = static_cast<float>(e2.longitude_);

   // TODO: Base X/Y offsets in pixels
   // const float x1 = static_cast<float>(e1.x_);
   // const float y1 = static_cast<float>(e1.y_);
   // const float x2 = static_cast<float>(e2.x_);
   // const float y2 = static_cast<float>(e2.y_);

   // Angle
   const float a = angle;

   // Final X/Y offsets in pixels
   const float hw = width * 0.5f;
   const float lx = -hw;
   const float rx = +hw;
   const float ty = +hw;
   const float by = -hw;

   // Modulate color
   const float mc0 = color[0] / 255.0f;
   const float mc1 = color[1] / 255.0f;
   const float mc2 = color[2] / 255.0f;
   const float mc3 = color[3] / 255.0f;

   // Update buffers
   newLinesBuffer_.insert(newLinesBuffer_.end(),
                          {
                             // Line
                             lat1, lon1, lx, by, mc0, mc1, mc2, mc3, a, // BL
                             lat2, lon2, lx, ty, mc0, mc1, mc2, mc3, a, // TL
                             lat1, lon1, rx, by, mc0, mc1, mc2, mc3, a, // BR
                             lat1, lon1, rx, by, mc0, mc1, mc2, mc3, a, // BR
                             lat2, lon2, rx, ty, mc0, mc1, mc2, mc3, a, // TR
                             lat2, lon2, lx, ty, mc0, mc1, mc2, mc3, a  // TL
                          });
   newThresholdBuffer_.insert(
      newThresholdBuffer_.end(),
      {threshold, threshold, threshold, threshold, threshold, threshold});
}

void PlacefileLines::Impl::Update()
{
   // If the placefile has been updated
   if (dirty_)
   {
      gl::OpenGLFunctions& gl = context_->gl();

      // Buffer lines data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * currentLinesBuffer_.size(),
                      currentLinesBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * currentThresholdBuffer_.size(),
                      currentThresholdBuffer_.data(),
                      GL_DYNAMIC_DRAW);
   }

   dirty_ = false;
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
