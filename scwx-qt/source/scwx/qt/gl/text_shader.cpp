#include <scwx/qt/gl/text_shader.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#pragma warning(push, 0)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

namespace scwx
{
namespace qt
{
namespace gl
{

static const std::string logPrefix_ = "scwx::qt::gl::text_shader";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class TextShaderImpl
{
public:
   explicit TextShaderImpl(std::shared_ptr<GlContext> context) :
       context_ {context},
       shaderProgram_ {nullptr},
       projectionLocation_(GL_INVALID_INDEX)
   {
   }

   ~TextShaderImpl() {}

   std::shared_ptr<GlContext>     context_;
   std::shared_ptr<ShaderProgram> shaderProgram_;

   GLint projectionLocation_;
};

TextShader::TextShader(std::shared_ptr<GlContext> context) :
    p(std::make_unique<TextShaderImpl>(context))
{
}
TextShader::~TextShader() = default;

TextShader::TextShader(TextShader&&) noexcept            = default;
TextShader& TextShader::operator=(TextShader&&) noexcept = default;

bool TextShader::Initialize()
{
   OpenGLFunctions& gl = p->context_->gl();

   // Load and configure shader
   p->shaderProgram_ =
      p->context_->GetShaderProgram(":/gl/text.vert", ":/gl/text.frag");

   p->projectionLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "projection");
   if (p->projectionLocation_ == -1)
   {
      logger_->warn("Could not find projection");
   }

   return true;
}

void TextShader::RenderText(const std::string&               text,
                            float                            x,
                            float                            y,
                            float                            pointSize,
                            const glm::mat4&                 projection,
                            const boost::gil::rgba8_pixel_t& color,
                            std::shared_ptr<util::Font>      font,
                            GLuint                           textureId,
                            TextAlign                        align)
{
   OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_->Use();

   gl.glEnable(GL_BLEND);
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   gl.glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_2D, textureId);

   switch (align)
   {
   case TextAlign::Left:
      // Do nothing
      break;

   case TextAlign::Center:
      // X position is the center of text, subtract half length
      x -= font->TextLength(text, pointSize) * 0.5f;
      break;

   case TextAlign::Right:
      // X position is the end of text, subtract length
      x -= font->TextLength(text, pointSize);
      break;
   }

   std::shared_ptr<util::FontBuffer> buffer =
      std::make_shared<util::FontBuffer>();
   font->BufferText(buffer, text, x, y, pointSize, color);
   buffer->Render(gl);
}

void TextShader::SetProjection(const glm::mat4& projection)
{
   p->context_->gl().glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
}

} // namespace gl
} // namespace qt
} // namespace scwx
