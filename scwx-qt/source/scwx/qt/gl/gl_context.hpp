#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/gl/shader_program.hpp>

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

   gl::OpenGLFunctions& gl();

   std::shared_ptr<gl::ShaderProgram>
   GetShaderProgram(const std::string& vertexPath,
                    const std::string& fragmentPath);

   GLuint GetTextureAtlas();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace gl
} // namespace qt
} // namespace scwx
