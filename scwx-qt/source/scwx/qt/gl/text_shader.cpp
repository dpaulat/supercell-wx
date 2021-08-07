#include <scwx/qt/gl/text_shader.hpp>

#include <boost/log/trivial.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{

static const std::string logPrefix_ = "[scwx::qt::gl::text_shader] ";

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
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find projection";
   }

   return success;
}

void TextShader::RenderText(const std::string&               text,
                            float                            x,
                            float                            y,
                            float                            scale,
                            const glm::mat4&                 projection,
                            const boost::gil::rgba8_pixel_t& color,
                            std::shared_ptr<util::Font>      font,
                            GLuint                           textureId)
{
   OpenGLFunctions& gl = p->gl_;

   Use();

   gl.glEnable(GL_BLEND);
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   gl.glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_2D, textureId);

   std::shared_ptr<util::FontBuffer> buffer = util::Font::CreateBuffer();
   font->BufferText(buffer, text, x, y, scale, color);
   util::Font::RenderBuffer(gl, buffer);
}

void TextShader::SetProjection(const glm::mat4& projection)
{
   p->gl_.glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
}

} // namespace gl
} // namespace qt
} // namespace scwx
