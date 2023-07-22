#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/util/font_buffer.hpp>

#include <memory>
#include <string>

#include <boost/gil/typedefs.hpp>

struct ImFont;

namespace scwx
{
namespace qt
{
namespace util
{

class FontImpl;

class Font
{
public:
   explicit Font(const std::string& resource);
   ~Font();

   Font(const Font&)            = delete;
   Font& operator=(const Font&) = delete;

   Font(Font&&)            = delete;
   Font& operator=(Font&&) = delete;

   float BufferText(std::shared_ptr<FontBuffer> buffer,
                    const std::string&          text,
                    float                       x,
                    float                       y,
                    float                       pointSize,
                    boost::gil::rgba8_pixel_t   color) const;
   float Kerning(char c1, char c2) const;
   float TextLength(const std::string& text, float pointSize) const;

   ImFont* ImGuiFont(std::size_t fontPixelSize);

   GLuint GenerateTexture(gl::OpenGLFunctions& gl);

   static std::shared_ptr<Font> Create(const std::string& resource);

private:
   std::unique_ptr<FontImpl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
