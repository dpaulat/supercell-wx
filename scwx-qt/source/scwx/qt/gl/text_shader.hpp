#pragma once

#include <scwx/qt/util/shader_program.hpp>
#include <scwx/qt/util/font.hpp>

#include <memory>

#include <boost/gil.hpp>
#include <glm/glm.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{

class TextShaderImpl;

class TextShader : public ShaderProgram
{
public:
   explicit TextShader(OpenGLFunctions& gl);
   ~TextShader();

   TextShader(const TextShader&) = delete;
   TextShader& operator=(const TextShader&) = delete;

   TextShader(TextShader&&) noexcept;
   TextShader& operator=(TextShader&&) noexcept;

   bool Initialize();
   void RenderText(const std::string&                           text,
                   float                                        x,
                   float                                        y,
                   float                                        scale,
                   const glm::mat4&                             projection,
                   const boost::gil::rgba8_pixel_t&             color,
                   const std::unordered_map<char, util::Glyph>& glyphs);
   void SetProjection(const glm::mat4& projection);
   void SetTextColor(const boost::gil::rgba8_pixel_t color);

private:
   std::unique_ptr<TextShaderImpl> p;
};

} // namespace gl
} // namespace qt
} // namespace scwx
