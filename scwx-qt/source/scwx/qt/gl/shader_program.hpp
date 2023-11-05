#pragma once

#include <scwx/qt/gl/gl.hpp>

#ifdef _WIN32
#   include <Windows.h>
#endif

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace gl
{

class ShaderProgram
{
public:
   explicit ShaderProgram(OpenGLFunctions& gl);
   virtual ~ShaderProgram();

   ShaderProgram(const ShaderProgram&)            = delete;
   ShaderProgram& operator=(const ShaderProgram&) = delete;

   ShaderProgram(ShaderProgram&&) noexcept;
   ShaderProgram& operator=(ShaderProgram&&) noexcept;

   GLuint id() const;

   GLint GetUniformLocation(const std::string& name);

   bool Load(const std::string& vertexPath, const std::string& fragmentPath);
   bool Load(std::initializer_list<std::pair<GLenum, std::string>> shaderPaths);

   void Use() const;

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace gl
} // namespace qt
} // namespace scwx
