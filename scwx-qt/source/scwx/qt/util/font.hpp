#pragma once

#include <scwx/qt/util/gl.hpp>

#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace scwx
{
namespace qt
{
namespace util
{

struct Glyph
{
   GLuint     textureId;
   glm::ivec2 size;    // pixels
   glm::ivec2 bearing; // pixels
   GLint      advance; // 1/64 pixels
};

class FontImpl;

class Font
{
public:
   explicit Font(const std::string& resource);
   ~Font();

   Font(const Font&) = delete;
   Font& operator=(const Font&) = delete;

   Font(Font&&)  = delete;
   Font& operator=(Font&&) = delete;

   void GenerateGlyphs(OpenGLFunctions&                 gl,
                       std::unordered_map<char, Glyph>& glyphs,
                       unsigned int                     height);

   static std::shared_ptr<Font> Create(const std::string& resource);

private:
   std::unique_ptr<FontImpl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
