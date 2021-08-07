#include <scwx/qt/util/font.hpp>

#include <unordered_map>

#include <boost/log/trivial.hpp>
#include <boost/timer/timer.hpp>
#include <QFile>

// #include <freetype-gl.h> (exclude opengl.h)
#include <platform.h>
#include <vec234.h>
#include <vector.h>
#include <texture-atlas.h>
#include <texture-font.h>
#include <ftgl-utils.h>

namespace scwx
{
namespace qt
{
namespace util
{

struct TextureGlyph
{
   int   offsetX_;
   int   offsetY_;
   int   width_;
   int   height_;
   float s0_;
   float t0_;
   float s1_;
   float t1_;
   float advanceX_;

   TextureGlyph(int   offsetX,
                int   offsetY,
                int   width,
                int   height,
                float s0,
                float t0,
                float s1,
                float t1,
                float advanceX) :
       offsetX_ {offsetX},
       offsetY_ {offsetY},
       width_ {width},
       height_ {height},
       s0_ {s0},
       t0_ {t0},
       s1_ {s1},
       t1_ {t1},
       advanceX_ {advanceX}
   {
   }
};

static const std::string CODEPOINTS =
   " !\"#$%&'()*+,-./0123456789:;<=>?"
   "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
   "`abcdefghijklmnopqrstuvwxyz{|}~";

static const std::string logPrefix_ = "[scwx::qt::util::font] ";

static constexpr float BASE_POINT_SIZE = 72.0f;
static constexpr float POINT_SCALE     = 1.0f / BASE_POINT_SIZE;

static std::unordered_map<std::string, std::shared_ptr<Font>> fontMap_;

class FontImpl
{
public:
   explicit FontImpl(const std::string& resource) :
       resource_(resource), atlas_ {nullptr}
   {
   }

   ~FontImpl()
   {
      if (atlas_ != nullptr)
      {
         ftgl::texture_atlas_delete(atlas_);
      }
   }

   const std::string resource_;

   ftgl::texture_atlas_t*                 atlas_;
   std::unordered_map<char, TextureGlyph> glyphs_;
};

Font::Font(const std::string& resource) :
    p(std::make_unique<FontImpl>(resource))
{
}
Font::~Font() = default;

float Font::BufferText(std::shared_ptr<FontBuffer> buffer,
                       const std::string&          text,
                       float                       x,
                       float                       y,
                       float                       pointSize,
                       boost::gil::rgba8_pixel_t   color) const
{
   static constexpr float colorScale = 1.0f / 255.0f;

   const float scale = pointSize * POINT_SCALE;

   float r = color[0] * colorScale;
   float g = color[1] * colorScale;
   float b = color[2] * colorScale;
   float a = color[3] * colorScale;

   for (size_t i = 0; i < text.length(); ++i)
   {
      const char& c = text[i];

      auto it = p->glyphs_.find(c);
      if (it == p->glyphs_.end())
      {
         BOOST_LOG_TRIVIAL(info)
            << logPrefix_
            << "Could not draw character: " << static_cast<uint32_t>(c);
         continue;
      }

      TextureGlyph& glyph = it->second;

      if (i > 0)
      {
         x += Kerning(text[i - 1], c) * scale;
      }

      float x0 = x + glyph.offsetX_ * scale;
      float y0 = y + glyph.offsetY_ * scale;
      float x1 = x0 + glyph.width_ * scale;
      float y1 = y0 - glyph.height_ * scale;

      float s0 = glyph.s0_;
      float t0 = glyph.t0_;
      float s1 = glyph.s1_;
      float t1 = glyph.t1_;

      buffer->Push(/* Indices  */ {0, 1, 2, 0, 2, 3},             //
                   /* Vertices */ {x0, y0, 0, s0, t0, r, g, b, a, //
                                   x0, y1, 0, s0, t1, r, g, b, a, //
                                   x1, y1, 0, s1, t1, r, g, b, a, //
                                   x1, y0, 0, s1, t0, r, g, b, a});

      x += glyph.advanceX_ * scale;
   }

   return x;
}

float Font::Kerning(char c1, char c2) const
{
   // TODO
   return 0.0f;
}

float Font::TextLength(const std::string& text, float pointSize) const
{
   const float scale = pointSize * POINT_SCALE;

   float x = 0.0f;

   for (size_t i = 0; i < text.length(); ++i)
   {
      const char& c = text[i];

      auto it = p->glyphs_.find(c);
      if (it == p->glyphs_.end())
      {
         BOOST_LOG_TRIVIAL(info)
            << logPrefix_
            << "Character not found: " << static_cast<uint32_t>(c);
         continue;
      }

      TextureGlyph& glyph = it->second;

      if (i > 0)
      {
         x += Kerning(text[i - 1], c) * scale;
      }

      x += glyph.advanceX_ * scale;
   }

   return x;
}

GLuint Font::GenerateTexture(OpenGLFunctions& gl)
{
   gl.glGenTextures(1, &p->atlas_->id);
   gl.glBindTexture(GL_TEXTURE_2D, p->atlas_->id);

   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   gl.glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RED,
                   static_cast<GLsizei>(p->atlas_->width),
                   static_cast<GLsizei>(p->atlas_->height),
                   0,
                   GL_RED,
                   GL_UNSIGNED_BYTE,
                   p->atlas_->data);

   return p->atlas_->id;
}

std::shared_ptr<Font> Font::Create(const std::string& resource)
{
   std::shared_ptr<Font>   font = nullptr;
   boost::timer::cpu_timer timer;

   auto it = fontMap_.find(resource);
   if (it != fontMap_.end())
   {
      return it->second;
   }

   QFile fontFile(resource.c_str());
   fontFile.open(QIODevice::ReadOnly);
   if (!fontFile.isOpen())
   {
      BOOST_LOG_TRIVIAL(error)
         << logPrefix_ << "Could not read font file: " << resource;
      return font;
   }

   font                = std::make_shared<Font>(resource);
   QByteArray fontData = fontFile.readAll();

   font->p->atlas_                   = ftgl::texture_atlas_new(512, 512, 1);
   ftgl::texture_font_t* textureFont = ftgl::texture_font_new_from_memory(
      font->p->atlas_, BASE_POINT_SIZE, fontData.constData(), fontData.size());

   textureFont->rendermode = ftgl::RENDER_SIGNED_DISTANCE_FIELD;

   timer.start();
   texture_font_load_glyphs(textureFont, CODEPOINTS.c_str());
   timer.stop();

   // Single-byte UTF-8 characters
   for (const char& c : CODEPOINTS)
   {
      const ftgl::texture_glyph_t* glyph =
         ftgl::texture_font_get_glyph(textureFont, &c);

      if (glyph != nullptr)
      {
         font->p->glyphs_.emplace(c,
                                  TextureGlyph(glyph->offset_x,
                                               glyph->offset_y,
                                               static_cast<int>(glyph->width),
                                               static_cast<int>(glyph->height),
                                               glyph->s0,
                                               glyph->t0,
                                               glyph->s1,
                                               glyph->t1,
                                               glyph->advance_x));
      }
   }

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Font \"" << resource
                            << "\" loaded in " << timer.format(6, "%ws");

   texture_font_delete(textureFont);

   if (font != nullptr)
   {
      fontMap_.insert({resource, font});
   }

   return font;
}

} // namespace util
} // namespace qt
} // namespace scwx
