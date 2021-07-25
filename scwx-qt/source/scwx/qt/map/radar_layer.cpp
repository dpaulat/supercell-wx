#include <scwx/qt/map/radar_layer.hpp>
#include <scwx/qt/util/shader_program.hpp>

#include <execution>

#include <boost/log/trivial.hpp>
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
   explicit RadarLayerImpl(std::shared_ptr<view::RadarView> radarView,
                           OpenGLFunctions&                 gl) :
       radarView_(radarView),
       gl_(gl),
       shaderProgram_(gl),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
   }
   ~RadarLayerImpl() = default;

   std::shared_ptr<view::RadarView> radarView_;
   OpenGLFunctions&                 gl_;

   ShaderProgram shaderProgram_;
   GLint         uMVPMatrixLocation_;
   GLint         uMapScreenCoordLocation_;
   GLuint        vbo_;
   GLuint        vao_;

   GLsizeiptr numVertices_;
};

RadarLayer::RadarLayer(std::shared_ptr<view::RadarView> radarView,
                       OpenGLFunctions&                 gl) :
    p(std::make_unique<RadarLayerImpl>(radarView, gl))
{
}
RadarLayer::~RadarLayer() = default;

RadarLayer::RadarLayer(RadarLayer&&) noexcept = default;
RadarLayer& RadarLayer::operator=(RadarLayer&&) noexcept = default;

void RadarLayer::initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "initialize()";

   OpenGLFunctions& gl = p->gl_;

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

   const std::vector<float>& vertices = p->radarView_->vertices();

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
                   vertices.size() * sizeof(GLfloat),
                   vertices.data(),
                   GL_STATIC_DRAW);
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Vertices buffered in " << timer.format(6, "%ws");

   // Set the vertex attributes pointers
   gl.glVertexAttribPointer(
      0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   p->numVertices_ = vertices.size();
}

void RadarLayer::render(const QMapbox::CustomLayerRenderParameters& params)
{
   OpenGLFunctions& gl = p->gl_;

   p->shaderProgram_.Use();

   const float scale = p->radarView_->scale() * 2.0f * mbgl::util::tileSize /
                       mbgl::util::DEGREES_MAX;
   const float xScale = scale / params.width;
   const float yScale = scale / params.height;

   glm::mat4 uMVPMatrix(1.0f);
   uMVPMatrix = glm::scale(uMVPMatrix, glm::vec3(xScale, yScale, 1.0f));
   uMVPMatrix = glm::rotate(uMVPMatrix,
                            glm::radians<float>(params.bearing),
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
   OpenGLFunctions& gl = p->gl_;

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
