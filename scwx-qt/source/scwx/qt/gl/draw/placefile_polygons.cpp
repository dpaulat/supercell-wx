#include <scwx/qt/gl/draw/placefile_polygons.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#include <mutex>

#include <GL/glu.h>
#include <boost/container/stable_vector.hpp>

#if defined(_WIN32)
typedef void (*_GLUfuncptr)(void);
#endif

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_polygons";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kVerticesPerTriangle = 3;
static constexpr std::size_t kPointsPerVertex     = 8;

// Threshold, start time, end time
static constexpr std::size_t kIntegersPerVertex_ = 3;

static constexpr std::size_t kTessVertexScreenX_ = 0;
static constexpr std::size_t kTessVertexScreenY_ = 1;
static constexpr std::size_t kTessVertexScreenZ_ = 2;
static constexpr std::size_t kTessVertexXOffset_ = 3;
static constexpr std::size_t kTessVertexYOffset_ = 4;
static constexpr std::size_t kTessVertexR_       = 5;
static constexpr std::size_t kTessVertexG_       = 6;
static constexpr std::size_t kTessVertexB_       = 7;
static constexpr std::size_t kTessVertexA_       = 8;
static constexpr std::size_t kTessVertexSize_    = kTessVertexA_ + 1;

typedef std::array<GLdouble, kTessVertexSize_> TessVertexArray;

class PlacefilePolygons::Impl
{
public:
   explicit Impl(const std::shared_ptr<GlContext>& context) :
       context_ {context},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       uMapDistanceLocation_(GL_INVALID_INDEX),
       uSelectedTimeLocation_(GL_INVALID_INDEX),
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
      tessellator_ = gluNewTess();

      gluTessCallback(tessellator_, //
                      GLU_TESS_COMBINE_DATA,
                      (_GLUfuncptr) &TessellateCombineCallback);
      gluTessCallback(tessellator_, //
                      GLU_TESS_VERTEX_DATA,
                      (_GLUfuncptr) &TessellateVertexCallback);

      // Force GLU_TRIANGLES
      gluTessCallback(tessellator_, //
                      GLU_TESS_EDGE_FLAG,
                      []() {});

      gluTessCallback(tessellator_, //
                      GLU_TESS_ERROR,
                      (_GLUfuncptr) &TessellateErrorCallback);
   }

   ~Impl() { gluDeleteTess(tessellator_); }

   void Update();

   void Tessellate(const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di);

   static void TessellateCombineCallback(GLdouble coords[3],
                                         void*    vertexData[4],
                                         GLfloat  weight[4],
                                         void**   outData,
                                         void*    polygonData);
   static void TessellateVertexCallback(void* vertexData, void* polygonData);
   static void TessellateErrorCallback(GLenum errorCode);

   std::shared_ptr<GlContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   boost::container::stable_vector<TessVertexArray> tessCombineBuffer_ {};

   std::mutex           bufferMutex_ {};
   std::vector<GLfloat> currentBuffer_ {};
   std::vector<GLint>   currentIntegerBuffer_ {};
   std::vector<GLfloat> newBuffer_ {};
   std::vector<GLint>   newIntegerBuffer_ {};

   GLUtesselator* tessellator_;

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;
   GLint                          uSelectedTimeLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;

   GLint currentThreshold_ {};
   GLint currentStartTime_ {};
   GLint currentEndTime_ {};
};

PlacefilePolygons::PlacefilePolygons(
   const std::shared_ptr<GlContext>& context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
PlacefilePolygons::~PlacefilePolygons() = default;

PlacefilePolygons::PlacefilePolygons(PlacefilePolygons&&) noexcept = default;
PlacefilePolygons&
PlacefilePolygons::operator=(PlacefilePolygons&&) noexcept = default;

void PlacefilePolygons::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void PlacefilePolygons::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefilePolygons::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/map_color.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/color.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");
   p->uMapMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMapMatrix");
   p->uMapScreenCoordLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapScreenCoord");
   p->uMapDistanceLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapDistance");
   p->uSelectedTimeLocation_ =
      p->shaderProgram_->GetUniformLocation("uSelectedTime");

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(2, p->vbo_.data());

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aScreenCoord
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

   // aColor
   gl.glVertexAttribPointer(2,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(2);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aThreshold
   gl.glVertexAttribIPointer(3, //
                             1,
                             GL_INT,
                             kIntegersPerVertex_ * sizeof(GLint),
                             static_cast<void*>(0));
   gl.glEnableVertexAttribArray(3);

   // aTimeRange
   gl.glVertexAttribIPointer(4, //
                             2,
                             GL_INT,
                             kIntegersPerVertex_ * sizeof(GLint),
                             reinterpret_cast<void*>(1 * sizeof(GLint)));
   gl.glEnableVertexAttribArray(4);

   p->dirty_ = true;
}

void PlacefilePolygons::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   if (!p->currentBuffer_.empty())
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

      // Selected time
      std::chrono::system_clock::time_point selectedTime =
         (p->selectedTime_ == std::chrono::system_clock::time_point {}) ?
            std::chrono::system_clock::now() :
            p->selectedTime_;
      gl.glUniform1i(
         p->uSelectedTimeLocation_,
         static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                               selectedTime.time_since_epoch())
                               .count()));

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefilePolygons::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(2, p->vbo_.data());

   std::unique_lock lock {p->bufferMutex_};

   // Clear the current buffers
   p->currentBuffer_.clear();
   p->currentIntegerBuffer_.clear();
}

void PlacefilePolygons::StartPolygons()
{
   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();
}

void PlacefilePolygons::AddPolygon(
   const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di)
{
   if (di != nullptr)
   {
      p->Tessellate(di);
   }
}

void PlacefilePolygons::FinishPolygons()
{
   std::unique_lock lock {p->bufferMutex_};

   // Swap buffers
   p->currentBuffer_.swap(p->newBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);

   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefilePolygons::Impl::Update()
{
   if (dirty_)
   {
      gl::OpenGLFunctions& gl = context_->gl();

      std::unique_lock lock {bufferMutex_};

      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLfloat) * currentBuffer_.size(),
                      currentBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * currentIntegerBuffer_.size(),
                      currentIntegerBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      numVertices_ =
         static_cast<GLsizei>(currentBuffer_.size() / kPointsPerVertex);

      dirty_ = false;
   }
}

void PlacefilePolygons::Impl::Tessellate(
   const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di)
{
   // Vertex storage
   boost::container::stable_vector<TessVertexArray> vertices {};

   // Default color to "Color" statement
   boost::gil::rgba8_pixel_t lastColor = di->color_;

   // Current threshold
   units::length::nautical_miles<double> threshold = di->threshold_;
   currentThreshold_ = static_cast<GLint>(std::round(threshold.value()));

   // Start and end time
   currentStartTime_ =
      static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                            di->startTime_.time_since_epoch())
                            .count());
   currentEndTime_ =
      static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                            di->endTime_.time_since_epoch())
                            .count());

   gluTessBeginPolygon(tessellator_, this);

   for (auto& contour : di->contours_)
   {
      gluTessBeginContour(tessellator_);

      for (auto& element : contour)
      {
         // Calculate screen coordinate
         auto screenCoordinate = util::maplibre::LatLongToScreenCoordinate(
            {element.latitude_, element.longitude_});

         // Update the most recent color if specified
         if (element.color_.has_value())
         {
            lastColor = element.color_.value();
         }

         // Add vertex to temporary storage
         auto& vertex =
            vertices.emplace_back(TessVertexArray {screenCoordinate.x,
                                                   screenCoordinate.y,
                                                   0.0, // z
                                                   element.x_,
                                                   element.y_,
                                                   lastColor[0] / 255.0,
                                                   lastColor[1] / 255.0,
                                                   lastColor[2] / 255.0,
                                                   lastColor[3] / 255.0});

         // Tessellate vertex
         gluTessVertex(tessellator_, vertex.data(), vertex.data());
      }

      gluTessEndContour(tessellator_);
   }

   gluTessEndPolygon(tessellator_);

   // Clear temporary storage
   tessCombineBuffer_.clear();

   // Remove extra vertices that don't correspond to a full triangle
   while (newBuffer_.size() % kVerticesPerTriangle != 0)
   {
      newBuffer_.pop_back();
      newIntegerBuffer_.pop_back();
   }
}

void PlacefilePolygons::Impl::TessellateCombineCallback(GLdouble coords[3],
                                                        void*    vertexData[4],
                                                        GLfloat  w[4],
                                                        void**   outData,
                                                        void*    polygonData)
{
   static constexpr std::size_t r = kTessVertexR_;
   static constexpr std::size_t a = kTessVertexA_;

   Impl* self = static_cast<Impl*>(polygonData);

   // Create new vertex data with given coordinates and interpolated color
   auto& newVertexData = self->tessCombineBuffer_.emplace_back( //
      TessVertexArray {
         coords[0],
         coords[1],
         coords[2],
         0.0, // offsetX
         0.0, // offsetY
         0.0, // r
         0.0, // g
         0.0, // b
         0.0  // a
      });

   for (std::size_t i = 0; i < 4; ++i)
   {
      GLdouble* d = static_cast<GLdouble*>(vertexData[i]);
      if (d != nullptr)
      {
         for (std::size_t color = r; color <= a; ++color)
         {
            newVertexData[color] += w[i] * d[color];
         }
      }
   }

   // Return new vertex data
   *outData = &newVertexData;
}

void PlacefilePolygons::Impl::TessellateVertexCallback(void* vertexData,
                                                       void* polygonData)
{
   Impl*     self = static_cast<Impl*>(polygonData);
   GLdouble* data = static_cast<GLdouble*>(vertexData);

   // Buffer vertex
   self->newBuffer_.insert(self->newBuffer_.end(),
                           {static_cast<float>(data[kTessVertexScreenX_]),
                            static_cast<float>(data[kTessVertexScreenY_]),
                            static_cast<float>(data[kTessVertexXOffset_]),
                            static_cast<float>(data[kTessVertexYOffset_]),
                            static_cast<float>(data[kTessVertexR_]),
                            static_cast<float>(data[kTessVertexG_]),
                            static_cast<float>(data[kTessVertexB_]),
                            static_cast<float>(data[kTessVertexA_])});
   self->newIntegerBuffer_.insert(self->newIntegerBuffer_.end(),
                                  {self->currentThreshold_,
                                   self->currentStartTime_,
                                   self->currentEndTime_});
}

void PlacefilePolygons::Impl::TessellateErrorCallback(GLenum errorCode)
{
   logger_->error("GL Error: {}", errorCode);
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
