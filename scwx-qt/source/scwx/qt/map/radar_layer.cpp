#include <scwx/qt/map/radar_layer.hpp>
#include <scwx/qt/util/shader_program.hpp>

#include <QOpenGLFunctions_3_3_Core>

#include <boost/log/trivial.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx
{
namespace qt
{

static const std::string logPrefix_ = "[scwx::qt::map::radar_layer] ";

class RadarLayerImpl
{
public:
   explicit RadarLayerImpl(std::shared_ptr<QMapboxGL> map) :
       map_(map),
       gl_(),
       shaderProgram_(),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
      gl_.initializeOpenGLFunctions();
   }
   ~RadarLayerImpl() = default;

   std::shared_ptr<QMapboxGL> map_;

   QOpenGLFunctions_3_3_Core gl_;

   ShaderProgram shaderProgram_;
   GLint         uMVPMatrixLocation_;
   GLuint        vbo_;
   GLuint        vao_;

   GLsizeiptr numVertices_;
};

RadarLayer::RadarLayer(std::shared_ptr<QMapboxGL> map) :
    p(std::make_unique<RadarLayerImpl>(map))
{
}
RadarLayer::~RadarLayer() = default;

RadarLayer::RadarLayer(RadarLayer&&) noexcept = default;
RadarLayer& RadarLayer::operator=(RadarLayer&&) noexcept = default;

void RadarLayer::initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "initialize()";

   QOpenGLFunctions_3_3_Core& gl = p->gl_;

   p->shaderProgram_.Load(":/gl/radar.vert", ":/gl/radar.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find uMVPMatrix";
   }
   constexpr uint16_t MAX_RADIALS           = 720;
   constexpr uint16_t MAX_DATA_MOMENT_GATES = 1840;

   static std::array<GLfloat, MAX_RADIALS * MAX_DATA_MOMENT_GATES * 6 * 2>
      vertices;

   constexpr float angleDelta  = glm::radians<float>(0.5f);
   constexpr float angleDeltaH = angleDelta / 2.0f;

   float angle1 = -angleDeltaH;
   float angle2 = angleDeltaH;

   GLsizeiptr index = 0;

   for (uint16_t azimuth = 0; azimuth < 720; ++azimuth)
   {
      const float dataMomentRange     = 2.125f;
      const float dataMomentInterval  = 0.25f;
      const float dataMomentIntervalH = dataMomentInterval * 0.5f;
      const float snrThreshold        = 2.0f;

      const uint16_t numberOfDataMomentGates = 1832;

      float range1 = dataMomentRange - dataMomentIntervalH;
      float range2 = range1 + dataMomentInterval;

      float sinTheta1 = std::sinf(angle1);
      float sinTheta2 = std::sinf(angle2);
      float cosTheta1 = std::cosf(angle1);
      float cosTheta2 = std::cosf(angle2);

      for (uint16_t gate = 0; gate < numberOfDataMomentGates; ++gate)
      {
         float r1SinTheta1 = range1 * sinTheta1;
         float r1SinTheta2 = range1 * sinTheta2;
         float r2SinTheta1 = range2 * sinTheta1;
         float r2SinTheta2 = range2 * sinTheta2;

         float r1CosTheta1 = range1 * cosTheta1;
         float r1CosTheta2 = range1 * cosTheta2;
         float r2CosTheta1 = range2 * cosTheta1;
         float r2CosTheta2 = range2 * cosTheta2;

         vertices[index++] = r1SinTheta1;
         vertices[index++] = r1CosTheta1;

         vertices[index++] = r2SinTheta1;
         vertices[index++] = r2CosTheta1;

         vertices[index++] = r1SinTheta2;
         vertices[index++] = r1CosTheta2;

         vertices[index++] = r1SinTheta2;
         vertices[index++] = r1CosTheta2;

         vertices[index++] = r2SinTheta2;
         vertices[index++] = r2CosTheta2;

         vertices[index++] = r2SinTheta1;
         vertices[index++] = r2CosTheta1;

         range1 += dataMomentInterval;
         range2 += dataMomentInterval;
      }

      angle1 += angleDelta;
      angle2 += angleDelta;
      break;
   }

   // Generate a vertex buffer object
   gl.glGenBuffers(1, &p->vbo_);

   // Generate a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);

   // Bind vertex array object
   gl.glBindVertexArray(p->vao_);

   // Copy vertices array in a buffer for OpenGL to use
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   gl.glBufferData(GL_ARRAY_BUFFER,
                   index * sizeof(GLfloat),
                   vertices.data(),
                   GL_STATIC_DRAW);

   // Set the vertex attributes pointers
   gl.glVertexAttribPointer(
      0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   p->numVertices_ = index;
}

void RadarLayer::render(const QMapbox::CustomLayerRenderParameters& params)
{
   QOpenGLFunctions_3_3_Core& gl = p->gl_;

   p->shaderProgram_.Use();

   const QMapbox::Coordinate radar(38.6986, -90.6828);

   const float metersPerPixel =
      QMapbox::metersPerPixelAtLatitude(params.latitude, params.zoom);

   const float scale  = 1000.0f / metersPerPixel * 2.0f;
   const float xScale = scale / params.width;
   const float yScale = scale / params.height;

   QPointF     radarScreen = p->map_->pixelForCoordinate(radar);
   const float xTranslate =
      (radarScreen.x() - (params.width * 0.5f)) / params.width * 2.0f;
   const float yTranslate =
      -(radarScreen.y() - (params.height * 0.5f)) / params.height * 2.0f;

   glm::mat4 uMVPMatrix(1.0f);
   uMVPMatrix =
      glm::translate(uMVPMatrix, glm::vec3(xTranslate, yTranslate, 0.0f));
   uMVPMatrix = glm::scale(uMVPMatrix, glm::vec3(xScale, yScale, 1.0f));
   uMVPMatrix = glm::rotate(uMVPMatrix,
                            glm::radians<float>(params.bearing),
                            glm::vec3(0.0f, 0.0f, 1.0f));

   gl.glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(uMVPMatrix));

   gl.glBindVertexArray(p->vao_);
   gl.glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   gl.glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void RadarLayer::deinitialize()
{
   QOpenGLFunctions_3_3_Core& gl = p->gl_;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "deinitialize()";

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(1, &p->vbo_);

   p->uMVPMatrixLocation_ = GL_INVALID_INDEX;
   p->vao_                = GL_INVALID_INDEX;
   p->vbo_                = GL_INVALID_INDEX;
}

} // namespace qt
} // namespace scwx
