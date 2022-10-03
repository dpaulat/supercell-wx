#include <scwx/qt/map/color_table_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::color_table_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ColorTableLayerImpl
{
public:
   explicit ColorTableLayerImpl(std::shared_ptr<MapContext> context) :
       shaderProgram_(nullptr),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       texture_ {GL_INVALID_INDEX},
       colorTableNeedsUpdate_ {true}
   {
   }
   ~ColorTableLayerImpl() = default;

   std::shared_ptr<gl::ShaderProgram> shaderProgram_;

   GLint                 uMVPMatrixLocation_;
   std::array<GLuint, 2> vbo_;
   GLuint                vao_;
   GLuint                texture_;

   std::vector<boost::gil::rgba8_pixel_t> colorTable_;

   bool colorTableNeedsUpdate_;
};

ColorTableLayer::ColorTableLayer(std::shared_ptr<MapContext> context) :
    GenericLayer(context), p(std::make_unique<ColorTableLayerImpl>(context))
{
}
ColorTableLayer::~ColorTableLayer() = default;

void ColorTableLayer::Initialize()
{
   logger_->debug("Initialize()");

   gl::OpenGLFunctions& gl = context()->gl();

   // Load and configure overlay shader
   p->shaderProgram_ =
      context()->GetShaderProgram(":/gl/texture1d.vert", ":/gl/texture1d.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      logger_->warn("Could not find uMVPMatrix");
   }

   gl.glGenTextures(1, &p->texture_);

   p->shaderProgram_->Use();

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

   connect(context()->radar_product_view().get(),
           &view::RadarProductView::ColorTableUpdated,
           this,
           [=]() { p->colorTableNeedsUpdate_ = true; });
}

void ColorTableLayer::Render(const QMapbox::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl               = context()->gl();
   auto                 radarProductView = context()->radar_product_view();

   if (context()->radar_product_view() == nullptr ||
       !context()->radar_product_view()->IsInitialized())
   {
      // Defer rendering until view is initialized
      return;
   }

   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   p->shaderProgram_->Use();

   gl.glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   if (p->colorTableNeedsUpdate_)
   {
      p->colorTable_ = radarProductView->color_table();

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

   if (p->colorTable_.size() > 0 && radarProductView->sweep_time() !=
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

void ColorTableLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   gl::OpenGLFunctions& gl = context()->gl();

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
