#include <scwx/qt/map/radar_layer.hpp>
#include <scwx/qt/util/shader_program.hpp>

#include <execution>

#include <QOpenGLFunctions_3_3_Core>

#include <boost/log/trivial.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{

static constexpr uint32_t MAX_RADIALS           = 720;
static constexpr uint32_t MAX_DATA_MOMENT_GATES = 1840;

static const std::string logPrefix_ = "[scwx::qt::map::radar_layer] ";

static glm::vec2
LatLongToScreenCoordinate(const QMapbox::Coordinate& coordinate);

class RadarLayerImpl
{
public:
   explicit RadarLayerImpl(std::shared_ptr<QMapboxGL> map) :
       map_(map),
       gl_(),
       shaderProgram_(),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       scale_ {0.0},
       bearing_ {0.0},
       numVertices_ {0}
   {
      gl_.initializeOpenGLFunctions();
   }
   ~RadarLayerImpl() = default;

   std::shared_ptr<QMapboxGL> map_;

   QOpenGLFunctions_3_3_Core gl_;

   ShaderProgram shaderProgram_;
   GLint         uMVPMatrixLocation_;
   GLint         uMapScreenCoordLocation_;
   GLuint        vbo_;
   GLuint        vao_;

   double scale_;
   double bearing_;

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

   boost::timer::cpu_timer timer;

   // Load and configure radar shader
   p->shaderProgram_.Load(":/gl/radar.vert", ":/gl/radar.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find uMVPMatrix";
   }

   p->uMapScreenCoordLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uMapScreenCoord");
   if (p->uMapScreenCoordLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Could not find uMapScreenCoord";
   }

   // Calculate coordinates
   static std::array<GLfloat, MAX_RADIALS * MAX_DATA_MOMENT_GATES * 2>
      coordinates;

   GeographicLib::Geodesic geodesic(GeographicLib::Constants::WGS84_a(),
                                    GeographicLib::Constants::WGS84_f());

   const QMapbox::Coordinate radar(38.6986, -90.6828);
   auto                      radialGates =
      boost::irange<uint32_t>(0, MAX_RADIALS * MAX_DATA_MOMENT_GATES);

   timer.start();
   std::for_each(
      std::execution::par_unseq,
      radialGates.begin(),
      radialGates.end(),
      [&](uint32_t radialGate) {
         const uint16_t gate =
            static_cast<uint16_t>(radialGate % MAX_DATA_MOMENT_GATES);
         const uint16_t radial =
            static_cast<uint16_t>(radialGate / MAX_DATA_MOMENT_GATES);

         const float  angle  = radial * 0.5f - 0.25f;
         const float  range  = (gate + 1) * 250.0f;
         const size_t offset = radialGate * 2;

         double latitude;
         double longitude;

         geodesic.Direct(
            radar.first, radar.second, angle, range, latitude, longitude);

         coordinates[offset]     = latitude;
         coordinates[offset + 1] = longitude;
      });
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << "Coordinates calculated in " << timer.format(6, "%ws");

   // Calculate vertices
   static std::array<GLfloat, MAX_RADIALS * MAX_DATA_MOMENT_GATES * 6 * 2>
              vertices;
   GLsizeiptr index = 0;

   timer.start();
   for (uint16_t radial = 0; radial < 720; ++radial)
   {
      const float dataMomentRange     = 2.125f * 1000.0f;
      const float dataMomentInterval  = 0.25f * 1000.0f;
      const float dataMomentIntervalH = dataMomentInterval * 0.5f;
      const float snrThreshold        = 2.0f;

      const uint16_t startGate               = 7;
      const uint16_t numberOfDataMomentGates = 1832;
      const uint16_t endGate                 = std::min<uint16_t>(
         numberOfDataMomentGates + startGate, MAX_DATA_MOMENT_GATES - 1);

      for (uint16_t gate = startGate; gate < endGate; ++gate)
      {
         size_t offset1 = (radial * MAX_DATA_MOMENT_GATES + gate) * 2;
         size_t offset2 = offset1 + 2;
         size_t offset3 =
            (((radial + 1) % MAX_RADIALS) * MAX_DATA_MOMENT_GATES + gate) * 2;
         size_t offset4 = offset3 + 2;

         vertices[index++] = coordinates[offset1];
         vertices[index++] = coordinates[offset1 + 1];

         vertices[index++] = coordinates[offset2];
         vertices[index++] = coordinates[offset2 + 1];

         vertices[index++] = coordinates[offset3];
         vertices[index++] = coordinates[offset3 + 1];

         vertices[index++] = coordinates[offset3];
         vertices[index++] = coordinates[offset3 + 1];

         vertices[index++] = coordinates[offset4];
         vertices[index++] = coordinates[offset4 + 1];

         vertices[index++] = coordinates[offset2];
         vertices[index++] = coordinates[offset2 + 1];
      }

      break;
   }
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << "Vertices calculated in " << timer.format(6, "%ws");

   // Generate a vertex buffer object
   gl.glGenBuffers(1, &p->vbo_);

   // Generate a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);

   // Bind vertex array object
   gl.glBindVertexArray(p->vao_);

   // Copy vertices array in a buffer for OpenGL to use
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   timer.start();
   gl.glBufferData(GL_ARRAY_BUFFER,
                   index * sizeof(GLfloat),
                   vertices.data(),
                   GL_STATIC_DRAW);
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << "Vertices buffered in " << timer.format(6, "%ws");

   // Set the vertex attributes pointers
   gl.glVertexAttribPointer(
      0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   p->numVertices_ = index;
   p->bearing_     = p->map_->bearing();
}

void RadarLayer::render(const QMapbox::CustomLayerRenderParameters& params)
{
   QOpenGLFunctions_3_3_Core& gl = p->gl_;

   p->shaderProgram_.Use();

   const float scale =
      p->map_->scale() * 2.0f * mbgl::util::tileSize / mbgl::util::DEGREES_MAX;
   const float xScale = scale / params.width;
   const float yScale = scale / params.height;

   glm::mat4 uMVPMatrix(1.0f);
   uMVPMatrix = glm::scale(uMVPMatrix, glm::vec3(xScale, yScale, 1.0f));
   uMVPMatrix = glm::rotate(uMVPMatrix,
                            glm::radians<float>(params.bearing - p->bearing_),
                            glm::vec3(0.0f, 0.0f, 1.0f));

   gl.glUniform2fv(p->uMapScreenCoordLocation_,
                   1,
                   glm::value_ptr(LatLongToScreenCoordinate(
                      {params.latitude, params.longitude})));

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

static glm::vec2
LatLongToScreenCoordinate(const QMapbox::Coordinate& coordinate)
{
   double latitude = std::clamp(
      coordinate.first, -mbgl::util::LATITUDE_MAX, mbgl::util::LATITUDE_MAX);
   glm::vec2 screen {
      mbgl::util::LONGITUDE_MAX + coordinate.second,
      -(mbgl::util::LONGITUDE_MAX -
        mbgl::util::RAD2DEG *
           std::log(std::tan(M_PI / 4.0 +
                             latitude * M_PI / mbgl::util::DEGREES_MAX)))};
   return screen;
}

} // namespace qt
} // namespace scwx
