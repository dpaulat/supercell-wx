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
   explicit Impl(std::shared_ptr<GlContext> context) :
       context_ {context},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
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

   std::vector<std::shared_ptr<const gr::Placefile::PolygonDrawItem>>
      polygonList_ {};

   boost::container::stable_vector<TessVertexArray> tessCombineBuffer_ {};

   std::mutex           bufferMutex_ {};
   std::vector<GLfloat> currentBuffer_ {};
   std::vector<GLfloat> newBuffer_ {};

   GLUtesselator* tessellator_;

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;

   GLuint vao_;
   GLuint vbo_;

   GLsizei numVertices_;

   boost::gil::rgba8_pixel_t currentColor_ {255, 255, 255, 255};
};

PlacefilePolygons::PlacefilePolygons(std::shared_ptr<GlContext> context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
PlacefilePolygons::~PlacefilePolygons() = default;

PlacefilePolygons::PlacefilePolygons(PlacefilePolygons&&) noexcept = default;
PlacefilePolygons&
PlacefilePolygons::operator=(PlacefilePolygons&&) noexcept = default;

void PlacefilePolygons::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ =
      p->context_->GetShaderProgram(":/gl/map_color.vert", ":/gl/color.frag");

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

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(1, &p->vbo_);

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
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

   p->dirty_ = true;
}

void PlacefilePolygons::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   if (!p->polygonList_.empty())
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);
      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);

      p->Update();
      p->shaderProgram_->Use();
      UseRotationProjection(params, p->uMVPMatrixLocation_);
      UseMapProjection(
         params, p->uMapMatrixLocation_, p->uMapScreenCoordLocation_);

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefilePolygons::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(1, &p->vbo_);
}

void PlacefilePolygons::StartPolygons()
{
   // Clear the new buffer
   p->newBuffer_.clear();

   // Clear the polygon list
   p->polygonList_.clear();
}

void PlacefilePolygons::AddPolygon(
   const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di)
{
   if (di != nullptr)
   {
      p->polygonList_.emplace_back(di);
      p->Tessellate(di);
   }
}

void PlacefilePolygons::FinishPolygons()
{
   std::unique_lock lock {p->bufferMutex_};

   // Swap buffers
   p->currentBuffer_.swap(p->newBuffer_);

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefilePolygons::Impl::Update()
{
   if (dirty_)
   {
      gl::OpenGLFunctions& gl = context_->gl();

      std::unique_lock lock {bufferMutex_};

      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLfloat) * currentBuffer_.size(),
                      currentBuffer_.data(),
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
}

void PlacefilePolygons::Impl::TessellateErrorCallback(GLenum errorCode)
{
   logger_->error("GL Error: {}", errorCode);
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
