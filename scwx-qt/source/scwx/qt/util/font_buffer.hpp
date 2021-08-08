#pragma once

#include <scwx/qt/gl/gl.hpp>

#include <memory>

namespace scwx
{
namespace qt
{
namespace util
{

class FontBufferImpl;

class FontBuffer
{
public:
   explicit FontBuffer();
   ~FontBuffer();

   FontBuffer(const FontBuffer&) = delete;
   FontBuffer& operator=(const FontBuffer&) = delete;

   FontBuffer(FontBuffer&&) = delete;
   FontBuffer& operator=(FontBuffer&&) = delete;

   void Clear();
   void Push(std::initializer_list<GLuint>  indices,
             std::initializer_list<GLfloat> vertices);
   void Render(gl::OpenGLFunctions& gl);

private:
   std::unique_ptr<FontBufferImpl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
