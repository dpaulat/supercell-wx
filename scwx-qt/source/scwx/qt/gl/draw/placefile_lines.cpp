#include <scwx/qt/gl/draw/placefile_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/imgui.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#include <execution>

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
   struct LineHoverEntry
   {
      std::shared_ptr<const gr::Placefile::LineDrawItem> di_;

      glm::vec2 p1_;
      glm::vec2 p2_;
      glm::vec2 otl_;
      glm::vec2 otr_;
      glm::vec2 obl_;
      glm::vec2 obr_;
   };

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

   void BufferLine(const std::shared_ptr<const gr::Placefile::LineDrawItem>& di,
                   const gr::Placefile::LineDrawItem::Element&               e1,
                   const gr::Placefile::LineDrawItem::Element&               e2,
                   const float                         width,
                   const units::angle::degrees<double> angle,
                   const boost::gil::rgba8_pixel_t     color,
                   const GLint                         threshold,
                   bool                                bufferHover = false);
   void
   UpdateBuffers(const std::shared_ptr<const gr::Placefile::LineDrawItem>& di);
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

   std::vector<LineHoverEntry> currentHoverLines_ {};
   std::vector<LineHoverEntry> newHoverLines_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;
};

PlacefileLines::PlacefileLines(const std::shared_ptr<GlContext>& context) :
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
   p->currentHoverLines_.clear();
}

void PlacefileLines::StartLines()
{
   // Clear the new buffers
   p->newLinesBuffer_.clear();
   p->newThresholdBuffer_.clear();
   p->newHoverLines_.clear();

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
   p->currentHoverLines_.swap(p->newHoverLines_);

   // Clear the new buffers
   p->newLinesBuffer_.clear();
   p->newThresholdBuffer_.clear();
   p->newHoverLines_.clear();

   // Update the number of lines
   p->currentNumLines_ = p->newNumLines_;
   p->numVertices_ =
      static_cast<GLsizei>(p->currentNumLines_ * kVerticesPerRectangle);

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileLines::Impl::UpdateBuffers(
   const std::shared_ptr<const gr::Placefile::LineDrawItem>& di)
{
   // Threshold value
   units::length::nautical_miles<double> threshold = di->threshold_;
   GLint thresholdValue = static_cast<GLint>(std::round(threshold.value()));

   std::vector<units::angle::degrees<double>> angles {};
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
      angles.push_back(angle);

      // Buffer line with hover text
      BufferLine(di,
                 di->elements_[i],
                 di->elements_[i + 1],
                 di->width_ + 2,
                 angle,
                 kBlack_,
                 thresholdValue,
                 true);
   }

   // For each element pair inside a Line statement, render a colored line
   for (std::size_t i = 0; i < di->elements_.size() - 1; ++i)
   {
      BufferLine(di,
                 di->elements_[i],
                 di->elements_[i + 1],
                 di->width_,
                 angles[i],
                 di->color_,
                 thresholdValue);
   }
}

void PlacefileLines::Impl::BufferLine(
   const std::shared_ptr<const gr::Placefile::LineDrawItem>& di,
   const gr::Placefile::LineDrawItem::Element&               e1,
   const gr::Placefile::LineDrawItem::Element&               e2,
   const float                                               width,
   const units::angle::degrees<double>                       angle,
   const boost::gil::rgba8_pixel_t                           color,
   const GLint                                               threshold,
   bool                                                      bufferHover)
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
   const float a = static_cast<float>(angle.value());

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

   if (bufferHover && !di->hoverText_.empty())
   {
      const units::angle::radians<double> radians = angle;

      const auto sc1 = util::maplibre::LatLongToScreenCoordinate({lat1, lon1});
      const auto sc2 = util::maplibre::LatLongToScreenCoordinate({lat2, lon2});

      const float cosAngle = cosf(static_cast<float>(radians.value()));
      const float sinAngle = sinf(static_cast<float>(radians.value()));

      const glm::mat2 rotate {cosAngle, -sinAngle, sinAngle, cosAngle};

      const glm::vec2 otl = rotate * glm::vec2 {-hw, +hw};
      const glm::vec2 otr = rotate * glm::vec2 {+hw, +hw};
      const glm::vec2 obl = rotate * glm::vec2 {-hw, -hw};
      const glm::vec2 obr = rotate * glm::vec2 {+hw, -hw};

      newHoverLines_.emplace_back(
         LineHoverEntry {di, sc1, sc2, otl, otr, obl, obr});
   }
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

bool PlacefileLines::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   const glm::vec2&                                mousePos)
{
   std::unique_lock lock {p->lineMutex_};

   bool itemPicked = false;

   // Calculate map scale, remove width and height from original calculation
   glm::vec2 scale = util::maplibre::GetMapScale(params);
   scale = 2.0f / glm::vec2 {scale.x * params.width, scale.y * params.height};

   // Scale and rotate the identity matrix to create the map matrix
   glm::mat4 mapMatrix {1.0f};
   mapMatrix = glm::scale(mapMatrix, glm::vec3 {scale, 1.0f});
   mapMatrix = glm::rotate(mapMatrix,
                           glm::radians<float>(params.bearing),
                           glm::vec3(0.0f, 0.0f, 1.0f));

   // For each pickable line
   auto it = std::find_if(
      std::execution::par_unseq,
      p->currentHoverLines_.crbegin(),
      p->currentHoverLines_.crend(),
      [&mapMatrix, &mousePos](const auto& line)
      {
         // Initialize vertices
         glm::vec2 bl = line.p1_;
         glm::vec2 br = bl;
         glm::vec2 tl = line.p2_;
         glm::vec2 tr = tl;

         // Calculate offsets
         // - Rotated offset is half the line width (pixels) in each direction
         // - Multiply the offset by the scaled and rotated map matrix
         const glm::vec2 otl = mapMatrix * glm::vec4 {line.otl_, 0.0f, 1.0f};
         const glm::vec2 obl = mapMatrix * glm::vec4 {line.obl_, 0.0f, 1.0f};
         const glm::vec2 obr = mapMatrix * glm::vec4 {line.obr_, 0.0f, 1.0f};
         const glm::vec2 otr = mapMatrix * glm::vec4 {line.otr_, 0.0f, 1.0f};

         // Offset vertices
         tl += otl;
         bl += obl;
         br += obr;
         tr += otr;

         // TODO: X/Y offsets

         // Test point against polygon bounds
         return util::maplibre::IsPointInPolygon({tl, bl, br, tr}, mousePos);
      });

   if (it != p->currentHoverLines_.crend())
   {
      itemPicked = true;
      util::ImGui::Instance().DrawTooltip(it->di_->hoverText_);
   }

   return itemPicked;
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
