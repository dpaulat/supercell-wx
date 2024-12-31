// Disable strncpy warning
#include <freetype/freetype.h>
#define _CRT_SECURE_NO_WARNINGS

#include <scwx/qt/types/imgui_font.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <limits>

#include <imgui.h>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::string logPrefix_ = "scwx::qt::types::imgui_font";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ImGuiFont::Impl
{
public:
   explicit Impl(const std::string&            fontName,
                 const std::vector<char>&      fontData,
                 units::font_size::pixels<int> size) :
       fontName_ {fontName}, size_ {size}, loaded_(HasUnicode(fontData))
   {
      if (!loaded_)
      {
         return;
      }
      CreateImGuiFont(fontData);
   }

   ~Impl() {}

   void CreateImGuiFont(const std::vector<char>& fontData);
   bool HasUnicode(const std::vector<char>& fontData);

   const std::string                   fontName_;
   const units::font_size::pixels<int> size_;
   bool                                loaded_;

   ImFont* imFont_ {nullptr};
};

ImGuiFont::ImGuiFont(const std::string&            fontName,
                     const std::vector<char>&      fontData,
                     units::font_size::pixels<int> size) :
    p(std::make_unique<Impl>(fontName, fontData, size))
{
}
ImGuiFont::~ImGuiFont() = default;

bool ImGuiFont::Impl::HasUnicode(const std::vector<char>& fontData)
{
   FT_Library ft_library;
   FT_Error   error = FT_Init_FreeType(&ft_library);
   if (error != 0)
   {
      logger_->error("Could not allocate ft_library");
      return false;
   }

   FT_Face Face;
   error = FT_New_Memory_Face(
      ft_library,
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(fontData.data())),
      static_cast<uint32_t>(fontData.size()),
      static_cast<uint32_t>(0),
      &Face);
   if (error != 0)
   {
      FT_Done_FreeType(ft_library);
      logger_->error("Could not allocate font face");
      return false;
   }
   error = FT_Select_Charmap(Face, FT_ENCODING_UNICODE);
   if (error != 0)
   {
      FT_Done_Face(Face);
      FT_Done_FreeType(ft_library);
      logger_->error("Font {} does not include a Unicode encoding", fontName_);
      return false;
   }
   FT_Done_Face(Face);
   FT_Done_FreeType(ft_library);

   return true;
}

void ImGuiFont::Impl::CreateImGuiFont(const std::vector<char>& fontData)
{
   logger_->debug("Creating Font: {}", fontName_);

   ImFontAtlas* fontAtlas = model::ImGuiContextModel::Instance().font_atlas();
   ImFontConfig fontConfig {};

   const float sizePixels = static_cast<float>(size_.value());

   // Do not transfer ownership of font data to ImGui, makes const_cast safe
   fontConfig.FontDataOwnedByAtlas = false;

   // Assign name to font
   strncpy(fontConfig.Name, fontName_.c_str(), sizeof(fontConfig.Name) - 1);
   fontConfig.Name[sizeof(fontConfig.Name) - 1] = 0;

   imFont_ = fontAtlas->AddFontFromMemoryTTF(
      const_cast<void*>(static_cast<const void*>(fontData.data())),
      static_cast<int>(std::clamp<std::size_t>(
         fontData.size(), 0, std::numeric_limits<int>::max())),
      sizePixels,
      &fontConfig);
}

ImFont* ImGuiFont::font()
{
   return p->imFont_;
}

bool ImGuiFont::loaded()
{
   return p->loaded_;
}

} // namespace types
} // namespace qt
} // namespace scwx
