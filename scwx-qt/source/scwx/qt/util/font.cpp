#include <scwx/qt/util/font.hpp>

#include <mutex>
#include <unordered_map>

#include <boost/log/trivial.hpp>
#include <ft2build.h>
#include <QFile>

#include FT_FREETYPE_H

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "[scwx::qt::util::font] ";

static std::unordered_map<std::string, std::shared_ptr<Font>> fontMap_;

static FT_Library ft_ {nullptr};
static std::mutex ftMutex_;

static bool InitializeFreeType();

class FontImpl
{
public:
   explicit FontImpl(const std::string& resource) :
       resource_(resource), fontData_(), face_ {nullptr}
   {
   }

   ~FontImpl() {}

   const std::string resource_;

   QByteArray fontData_;
   FT_Face    face_;
};

Font::Font(const std::string& resource) :
    p(std::make_unique<FontImpl>(resource))
{
}
Font::~Font()
{
   FT_Done_Face(p->face_);
}

void Font::GenerateGlyphs(OpenGLFunctions&                 gl,
                          std::unordered_map<char, Glyph>& glyphs,
                          unsigned int                     height)
{
   FT_Error error;
   FT_Face& face = p->face_;

   // Allow single-byte texture colors
   gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   FT_Set_Pixel_Sizes(p->face_, 0, 48);

   for (unsigned char c = 0; c < 128; c++)
   {
      if (glyphs.find(c) != glyphs.end())
      {
         BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Found glyph "
                                    << static_cast<uint16_t>(c) << ", skipping";
         continue;
      }

      if ((error = FT_Load_Char(face, c, FT_LOAD_RENDER)) != 0)
      {
         BOOST_LOG_TRIVIAL(error) << logPrefix_ << "Failed to load glyph "
                                  << static_cast<uint16_t>(c) << ": " << error;
         continue;
      }

      GLuint texture;
      gl.glGenTextures(1, &texture);
      gl.glBindTexture(GL_TEXTURE_2D, texture);

      gl.glTexImage2D(GL_TEXTURE_2D,
                      0,
                      GL_RED,
                      face->glyph->bitmap.width,
                      face->glyph->bitmap.rows,
                      0,
                      GL_RED,
                      GL_UNSIGNED_BYTE,
                      face->glyph->bitmap.buffer);

      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glyphs.insert(
         {c,
          Glyph {
             texture,
             glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
             glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
             face->glyph->advance.x}});
   }
}

std::shared_ptr<Font> Font::Create(const std::string& resource)
{
   std::shared_ptr<Font> font = nullptr;
   FT_Error              error;

   if (!InitializeFreeType())
   {
      return font;
   }

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

   font               = std::make_shared<Font>(resource);
   font->p->fontData_ = fontFile.readAll();

   {
      std::scoped_lock(ftMutex_);
      if ((error = FT_New_Memory_Face(
              ft_,
              reinterpret_cast<const FT_Byte*>(font->p->fontData_.data()),
              font->p->fontData_.size(),
              0,
              &font->p->face_)) != 0)
      {
         BOOST_LOG_TRIVIAL(error)
            << logPrefix_ << "Failed to load font: " << error;
         font.reset();
      }
   }

   if (font != nullptr)
   {
      fontMap_.insert({resource, font});
   }

   return font;
}

static bool InitializeFreeType()
{
   std::scoped_lock(ftMutex_);

   FT_Error error;

   if (ft_ == nullptr && (error = FT_Init_FreeType(&ft_)) != 0)
   {
      BOOST_LOG_TRIVIAL(error)
         << logPrefix_ << "Could not init FreeType library: " << error;
      ft_ = nullptr;
   }

   return (ft_ != nullptr);
}

} // namespace util
} // namespace qt
} // namespace scwx
