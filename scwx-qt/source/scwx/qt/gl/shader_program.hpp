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

   bool Load(const std::string& vertexPath, const std::string& fragmentPath);

   void Use() const;

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace gl
} // namespace qt
} // namespace scwx
