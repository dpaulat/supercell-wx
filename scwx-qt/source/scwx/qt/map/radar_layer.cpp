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
       texture_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
   }
   ~RadarLayerImpl() = default;

   std::shared_ptr<view::RadarView> radarView_;
   OpenGLFunctions&                 gl_;

   ShaderProgram         shaderProgram_;
   GLint                 uMVPMatrixLocation_;
   GLint                 uMapScreenCoordLocation_;
   std::array<GLuint, 2> vbo_;
   GLuint                vao_;
   GLuint                texture_;

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

   const std::vector<float>&    vertices      = p->radarView_->vertices();
   const std::vector<uint8_t>&  dataMoments8  = p->radarView_->data_moments8();
   const std::vector<uint16_t>& dataMoments16 = p->radarView_->data_moments16();

   // Generate and bind a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);
   gl.glBindVertexArray(p->vao_);

   // Generate vertex buffer objects
   gl.glGenBuffers(2, p->vbo_.data());

   // Buffer vertices
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   timer.start();
   gl.glBufferData(GL_ARRAY_BUFFER,
                   vertices.size() * sizeof(GLfloat),
                   vertices.data(),
                   GL_STATIC_DRAW);
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Vertices buffered in " << timer.format(6, "%ws");

   gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // Buffer data moments
   const GLvoid* data;
   GLsizeiptr    dataSize;
   GLenum        type;

   if (dataMoments8.size() > 0)
   {
      data     = static_cast<const GLvoid*>(dataMoments8.data());
      dataSize = dataMoments8.size() * sizeof(GLubyte);
      type     = GL_UNSIGNED_BYTE;
   }
   else
   {
      data     = static_cast<const GLvoid*>(dataMoments16.data());
      dataSize = dataMoments16.size() * sizeof(GLushort);
      type     = GL_UNSIGNED_SHORT;
   }

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   timer.start();
   gl.glBufferData(GL_ARRAY_BUFFER, dataSize, data, GL_STATIC_DRAW);
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Data moments buffered in " << timer.format(6, "%ws");

   gl.glVertexAttribIPointer(1, 1, type, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(1);

   p->numVertices_ = vertices.size() / 2;

   // Create color table
   gl.glGenTextures(1, &p->texture_);
   UpdateColorTable();
   gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
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

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_1D, p->texture_);
   gl.glBindVertexArray(p->vao_);
   gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
}

void RadarLayer::deinitialize()
{
   OpenGLFunctions& gl = p->gl_;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "deinitialize()";

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(2, p->vbo_.data());

   p->uMVPMatrixLocation_ = GL_INVALID_INDEX;
   p->vao_                = GL_INVALID_INDEX;
   p->vbo_                = {GL_INVALID_INDEX};
   p->texture_            = GL_INVALID_INDEX;
}

void RadarLayer::UpdateColorTable()
{
   OpenGLFunctions& gl = p->gl_;

   const std::vector<boost::gil::rgba8_pixel_t>& colorTable =
      p->radarView_->color_table();

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_1D, p->texture_);
   gl.glTexImage1D(GL_TEXTURE_1D,
                   0,
                   GL_RGBA,
                   (GLsizei) colorTable.size(),
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   colorTable.data());
   gl.glGenerateMipmap(GL_TEXTURE_1D);
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
