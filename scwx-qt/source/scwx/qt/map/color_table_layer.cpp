#include <scwx/qt/map/color_table_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>

#include <boost/log/trivial.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "[scwx::qt::map::color_table_layer] ";

class ColorTableLayerImpl
{
public:
   explicit ColorTableLayerImpl(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl) :
       radarProductView_(radarProductView),
       gl_(gl),
       shaderProgram_(gl),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       texture_ {GL_INVALID_INDEX},
       colorTableNeedsUpdate_ {true}
   {
   }
   ~ColorTableLayerImpl() = default;

   std::shared_ptr<view::RadarProductView> radarProductView_;
   gl::OpenGLFunctions&                    gl_;

   gl::ShaderProgram     shaderProgram_;
   GLint                 uMVPMatrixLocation_;
   std::array<GLuint, 2> vbo_;
   GLuint                vao_;
   GLuint                texture_;

   std::vector<boost::gil::rgba8_pixel_t> colorTable_;

   bool colorTableNeedsUpdate_;
};

ColorTableLayer::ColorTableLayer(
   std::shared_ptr<view::RadarProductView> radarProductView,
   gl::OpenGLFunctions&                    gl) :
    p(std::make_unique<ColorTableLayerImpl>(radarProductView, gl))
{
}
ColorTableLayer::~ColorTableLayer() = default;

void ColorTableLayer::initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "initialize()";

   gl::OpenGLFunctions& gl = p->gl_;

   // Load and configure overlay shader
   p->shaderProgram_.Load(":/gl/texture1d.vert", ":/gl/texture1d.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find uMVPMatrix";
   }

   gl.glGenTextures(1, &p->texture_);

   p->shaderProgram_.Use();

   // Generate a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);

   // Generate vertex buffer objects
   gl.glGenBuffers(2, p->vbo_.data());

   gl.glBindVertexArray(p->vao_);

   // Bottom panel
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_DYNAMIC_DRAW);

   gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // Color table panel texture coordinates
   const float textureCoords[6][1] = {{0.0f}, // TL
                                      {0.0f}, // BL
                                      {1.0f}, // TR
                                      //
                                      {0.0f},  // BL
                                      {1.0f},  // TR
                                      {1.0f}}; // BR
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(
      GL_ARRAY_BUFFER, sizeof(textureCoords), textureCoords, GL_STATIC_DRAW);

   gl.glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(1);

   connect(p->radarProductView_.get(),
           &view::RadarProductView::ColorTableUpdated,
           this,
           [=]() { p->colorTableNeedsUpdate_ = true; });
}

void ColorTableLayer::render(const QMapbox::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = p->gl_;

   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   p->shaderProgram_.Use();

   gl.glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   if (p->colorTableNeedsUpdate_)
   {
      p->colorTable_ = p->radarProductView_->color_table();

      gl.glActiveTexture(GL_TEXTURE0);
      gl.glBindTexture(GL_TEXTURE_1D, p->texture_);
      gl.glTexImage1D(GL_TEXTURE_1D,
                      0,
                      GL_RGBA,
                      (GLsizei) p->colorTable_.size(),
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      p->colorTable_.data());
      gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      gl.glGenerateMipmap(GL_TEXTURE_1D);
   }

   if (p->colorTable_.size() > 0 && p->radarProductView_->sweep_time() !=
                                       std::chrono::system_clock::time_point())
   {
      // Color table panel vertices
      const float vertexLX       = 0.0f;
      const float vertexRX       = static_cast<float>(params.width);
      const float vertexTY       = 10.0f;
      const float vertexBY       = 0.0f;
      const float vertices[6][2] = {{vertexLX, vertexTY}, // TL
                                    {vertexLX, vertexBY}, // BL
                                    {vertexRX, vertexTY}, // TR
                                    //
                                    {vertexLX, vertexBY},  // BL
                                    {vertexRX, vertexTY},  // TR
                                    {vertexRX, vertexBY}}; // BR

      // Draw vertices
      gl.glBindVertexArray(p->vao_);
      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
      gl.glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      gl.glDrawArrays(GL_TRIANGLES, 0, 6);
   }

   SCWX_GL_CHECK_ERROR();
}

void ColorTableLayer::deinitialize()
{
   gl::OpenGLFunctions& gl = p->gl_;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "deinitialize()";

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(2, p->vbo_.data());
   gl.glDeleteTextures(1, &p->texture_);

   p->uMVPMatrixLocation_ = GL_INVALID_INDEX;
   p->vao_                = GL_INVALID_INDEX;
   p->vbo_                = {GL_INVALID_INDEX};
   p->texture_            = GL_INVALID_INDEX;
}

} // namespace map
} // namespace qt
} // namespace scwx
