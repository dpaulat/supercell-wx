#include <scwx/qt/gl/text_shader.hpp>
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
   explicit TextShaderImpl(OpenGLFunctions& gl) :
       gl_ {gl}, projectionLocation_(GL_INVALID_INDEX)
   {
   }

   ~TextShaderImpl() {}

   OpenGLFunctions& gl_;

   GLint projectionLocation_;
};

TextShader::TextShader(OpenGLFunctions& gl) :
    ShaderProgram(gl), p(std::make_unique<TextShaderImpl>(gl))
{
}
TextShader::~TextShader() = default;

TextShader::TextShader(TextShader&&) noexcept = default;
TextShader& TextShader::operator=(TextShader&&) noexcept = default;

bool TextShader::Initialize()
{
   OpenGLFunctions& gl = p->gl_;

   // Load and configure shader
   bool success = Load(":/gl/text.vert", ":/gl/text.frag");

   p->projectionLocation_ = gl.glGetUniformLocation(id(), "projection");
   if (p->projectionLocation_ == -1)
   {
      logger_->warn("Could not find projection");
   }

   return success;
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
   OpenGLFunctions& gl = p->gl_;

   Use();

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
   p->gl_.glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
}

} // namespace gl
} // namespace qt
} // namespace scwx
