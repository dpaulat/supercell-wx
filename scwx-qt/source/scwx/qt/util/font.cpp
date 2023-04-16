// No suitable standard C++ replacement
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

// Disable strncpy warning
#define _CRT_SECURE_NO_WARNINGS

#include <scwx/qt/util/font.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/util/logger.hpp>

#include <codecvt>
#include <unordered_map>

#include <boost/timer/timer.hpp>
#include <imgui.h>
#include <QFile>
#include <QFileInfo>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

// #include <freetype-gl.h> (exclude opengl.h)
#include <platform.h>
#include <vec234.h>
#include <vector.h>
#include <texture-atlas.h>
#include <texture-font.h>
#include <ftgl-utils.h>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

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

static const std::string logPrefix_ = "scwx::qt::util::font";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr float BASE_POINT_SIZE = 72.0f;
static constexpr float POINT_SCALE     = 1.0f / BASE_POINT_SIZE;

static std::unordered_map<std::string, std::shared_ptr<Font>> fontMap_;

static void ParseSfntName(const FT_SfntName& sfntName, std::string& str);

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

   void CreateImGuiFont(QFile&                      fontFile,
                        QByteArray&                 fontData,
                        const std::vector<int64_t>& fontSizes);
   void ParseNames(FT_Face face);

   const std::string resource_;

   struct
   {
      std::string fontFamily_;
      std::string fontSubfamily_;
   } fontData_;

   ftgl::texture_atlas_t*                 atlas_;
   std::unordered_map<char, TextureGlyph> glyphs_;
   std::unordered_map<size_t, ImFont*>    imGuiFonts_;
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
         logger_->info("Could not draw character: {}",
                       static_cast<uint32_t>(c));
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

float Font::Kerning(char /*c1*/, char /*c2*/) const
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
         logger_->info("Character not found: {}", static_cast<uint32_t>(c));
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

GLuint Font::GenerateTexture(gl::OpenGLFunctions& gl)
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

void FontImpl::CreateImGuiFont(QFile&                      fontFile,
                               QByteArray&                 fontData,
                               const std::vector<int64_t>& fontSizes)
{
   QFileInfo    fileInfo(fontFile);
   ImFontAtlas* fontAtlas = model::ImGuiContextModel::Instance().font_atlas();
   ImFontConfig fontConfig {};

   // Do not transfer ownership of font data to ImGui, makes const_cast safe
   fontConfig.FontDataOwnedByAtlas = false;

   for (int64_t fontSize : fontSizes)
   {
      const float sizePixels = static_cast<float>(fontSize);

      // Assign name to font
      strncpy(fontConfig.Name,
              std::format("{}:{}", fileInfo.fileName().toStdString(), fontSize)
                 .c_str(),
              sizeof(fontConfig.Name));
      fontConfig.Name[sizeof(fontConfig.Name) - 1] = 0;

      // Add font to atlas
      imGuiFonts_.emplace(
         fontSize,
         fontAtlas->AddFontFromMemoryTTF(
            const_cast<void*>(static_cast<const void*>(fontData.constData())),
            fontData.size(),
            sizePixels,
            &fontConfig));
   }
}

std::shared_ptr<Font> Font::Create(const std::string& resource)
{
   logger_->debug("Loading font file: {}", resource);

   std::shared_ptr<Font>   font = nullptr;
   boost::timer::cpu_timer timer;

   auto it = fontMap_.find(resource);
   if (it != fontMap_.end())
   {
      logger_->debug("Font already created");
      return it->second;
   }

   QFile fontFile(resource.c_str());
   fontFile.open(QIODevice::ReadOnly);
   if (!fontFile.isOpen())
   {
      logger_->error("Could not read font file");
      return font;
   }

   font                = std::make_shared<Font>(resource);
   QByteArray fontData = fontFile.readAll();

   font->p->CreateImGuiFont(
      fontFile,
      fontData,
      manager::SettingsManager::general_settings().font_sizes().GetValue());

   font->p->atlas_                   = ftgl::texture_atlas_new(512, 512, 1);
   ftgl::texture_font_t* textureFont = ftgl::texture_font_new_from_memory(
      font->p->atlas_, BASE_POINT_SIZE, fontData.constData(), fontData.size());

   font->p->ParseNames(textureFont->face);

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

   logger_->debug("Font loaded in {}", timer.format(6, "%ws"));

   texture_font_delete(textureFont);

   if (font != nullptr)
   {
      fontMap_.insert({resource, font});
   }

   return font;
}

void FontImpl::ParseNames(FT_Face face)
{
   FT_SfntName sfntName;
   FT_Error    error;

   FT_UInt nameCount = FT_Get_Sfnt_Name_Count(face);

   for (FT_UInt i = 0; i < nameCount; i++)
   {
      error = FT_Get_Sfnt_Name(face, i, &sfntName);

      if (error == 0)
      {
         switch (sfntName.name_id)
         {
         case TT_NAME_ID_FONT_FAMILY:
            ParseSfntName(sfntName, fontData_.fontFamily_);
            break;

         case TT_NAME_ID_FONT_SUBFAMILY:
            ParseSfntName(sfntName, fontData_.fontSubfamily_);
            break;
         }
      }
   }

   logger_->debug(
      "Font family: {} ({})", fontData_.fontFamily_, fontData_.fontSubfamily_);
}

static void ParseSfntName(const FT_SfntName& sfntName, std::string& str)
{
   if (str.empty())
   {
      if (sfntName.platform_id == TT_PLATFORM_MICROSOFT &&
          sfntName.encoding_id == TT_MS_ID_UNICODE_CS)
      {
         char16_t* tempString = new char16_t[sfntName.string_len / 2];
         memcpy(tempString, sfntName.string, sfntName.string_len);

         for (size_t j = 0; j < sfntName.string_len / 2; j++)
         {
            tempString[j] = ntohs(tempString[j]);
         }

         str =
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}
               .to_bytes(tempString, tempString + sfntName.string_len / 2);

         delete[] tempString;
      }
      else if (sfntName.platform_id == TT_PLATFORM_MACINTOSH &&
               sfntName.encoding_id == TT_MAC_ID_ROMAN)
      {
         str = std::string(reinterpret_cast<char*>(sfntName.string),
                           sfntName.string_len);
      }
   }
}

} // namespace util
} // namespace qt
} // namespace scwx
