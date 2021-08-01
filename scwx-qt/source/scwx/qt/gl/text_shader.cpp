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
       gl_ {gl},
       projectionLocation_(GL_INVALID_INDEX),
       textColorLocation_(GL_INVALID_INDEX),
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX}
   {
   }

   ~TextShaderImpl() {}

   OpenGLFunctions& gl_;

   GLint projectionLocation_;
   GLint textColorLocation_;

   GLuint vao_;
   GLuint vbo_;
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

   p->textColorLocation_ = gl.glGetUniformLocation(id(), "textColor");
   if (p->textColorLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find textColor";
   }

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(1, &p->vbo_);
   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   gl.glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
   gl.glEnableVertexAttribArray(0);
   gl.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
   gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
   gl.glBindVertexArray(0);

   return success;
}

void TextShader::RenderText(const std::string&               text,
                            float                            x,
                            float                            y,
                            float                            scale,
                            const glm::mat4&                 projection,
                            const boost::gil::rgba8_pixel_t& color,
                            const std::unordered_map<char, util::Glyph>& glyphs)
{
   OpenGLFunctions& gl = p->gl_;

   Use();

   gl.glEnable(GL_BLEND);
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   gl.glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
   gl.glUniform4f(
      p->textColorLocation_, color[0], color[1], color[2], color[3]);

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindVertexArray(p->vao_);

   for (auto c = text.cbegin(); c != text.cend(); c++)
   {
      if (glyphs.find(*c) == glyphs.end())
      {
         continue;
      }

      const util::Glyph& g = glyphs.at(*c);

      float xpos = x + g.bearing.x * scale;
      float ypos = y - (g.size.y - g.bearing.y) * scale;

      float w = g.size.x * scale;
      float h = g.size.y * scale;

      // Glyph vertices
      float vertices[6][4] = {{xpos, ypos + h, 0.0f, 0.0f},
                              {xpos, ypos, 0.0f, 1.0f},
                              {xpos + w, ypos, 1.0f, 1.0f}, //
                                                            //
                              {xpos, ypos + h, 0.0f, 0.0f},
                              {xpos + w, ypos, 1.0f, 1.0f},
                              {xpos + w, ypos + h, 1.0f, 0.0f}};

      // Render glyph texture
      gl.glBindTexture(GL_TEXTURE_2D, g.textureId);

      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
      gl.glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      gl.glBindBuffer(GL_ARRAY_BUFFER, 0);

      gl.glDrawArrays(GL_TRIANGLES, 0, 6);

      // Advance to the next glyph
      x += (g.advance >> 6) * scale;
   }
}

void TextShader::SetProjection(const glm::mat4& projection)
{
   p->gl_.glUniformMatrix4fv(
      p->projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
}

void TextShader::SetTextColor(const boost::gil::rgba8_pixel_t color)
{
   p->gl_.glUniform4f(
      p->textColorLocation_, color[0], color[1], color[2], color[3]);
}

} // namespace gl
} // namespace qt
} // namespace scwx
