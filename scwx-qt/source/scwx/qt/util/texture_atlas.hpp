#pragma once

#include <scwx/qt/gl/gl.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace util
{

class TextureAtlas
{
public:
   explicit TextureAtlas();
   ~TextureAtlas();

   TextureAtlas(const TextureAtlas&)            = delete;
   TextureAtlas& operator=(const TextureAtlas&) = delete;

   TextureAtlas(TextureAtlas&&) noexcept;
   TextureAtlas& operator=(TextureAtlas&&) noexcept;

   static TextureAtlas& Instance();

   void   RegisterTexture(const std::string& name, const std::string& path);
   void   BuildAtlas(size_t width, size_t height);
   GLuint BufferAtlas(gl::OpenGLFunctions& gl);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
