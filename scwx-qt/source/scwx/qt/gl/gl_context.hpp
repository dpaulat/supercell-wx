#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/gl/shader_program.hpp>

#include <QOpenGLFunctions_3_0>

namespace scwx
{
namespace qt
{
namespace gl
{

class GlContext
{
public:
   explicit GlContext();
   virtual ~GlContext();

   GlContext(const GlContext&)            = delete;
   GlContext& operator=(const GlContext&) = delete;

   GlContext(GlContext&&) noexcept;
   GlContext& operator=(GlContext&&) noexcept;

   gl::OpenGLFunctions&  gl();
   QOpenGLFunctions_3_0& gl30();

   std::uint64_t texture_buffer_count() const;

   std::shared_ptr<gl::ShaderProgram>
   GetShaderProgram(const std::string& vertexPath,
                    const std::string& fragmentPath);
   std::shared_ptr<gl::ShaderProgram> GetShaderProgram(
      std::initializer_list<std::pair<GLenum, std::string>> shaders);

   GLuint GetTextureAtlas();

   void Initialize();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace gl
} // namespace qt
} // namespace scwx
