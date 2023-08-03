#pragma once

#include <scwx/qt/gl/gl.hpp>

#include <memory>
#include <string>

#include <boost/gil/point.hpp>

namespace scwx
{
namespace qt
{
namespace util
{

struct TextureAttributes
{
   TextureAttributes() :
       valid_ {false},
       position_ {},
       size_ {},
       sLeft_ {},
       sRight_ {},
       tTop_ {},
       tBottom_ {}
   {
   }

   TextureAttributes(boost::gil::point_t position,
                     boost::gil::point_t size,
                     float               sLeft,
                     float               sRight,
                     float               tTop,
                     float               tBottom) :
       valid_ {true},
       position_ {position},
       size_ {size},
       sLeft_ {sLeft},
       sRight_ {sRight},
       tTop_ {tTop},
       tBottom_ {tBottom}
   {
   }

   bool                valid_;
   boost::gil::point_t position_;
   boost::gil::point_t size_;
   float               sLeft_;
   float               sRight_;
   float               tTop_;
   float               tBottom_;
};

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

   std::uint64_t BuildCount() const;

   void   RegisterTexture(const std::string& name, const std::string& path);
   bool   CacheTexture(const std::string& name, const std::string& path);
   void   BuildAtlas(size_t width, size_t height);
   GLuint BufferAtlas(gl::OpenGLFunctions& gl);

   TextureAttributes GetTextureAttributes(const std::string& name);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
