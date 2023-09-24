// Disable strncpy warning
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
   explicit Impl(const std::string&    fontName,
                 const std::string&    fontData,
                 units::pixels<double> size) :
       fontName_ {fontName}, size_ {size}
   {
      CreateImGuiFont(fontData);
   }

   ~Impl() {}

   void CreateImGuiFont(const std::string& fontData);

   const std::string           fontName_;
   const units::pixels<double> size_;

   ImFont* imFont_ {nullptr};
};

ImGuiFont::ImGuiFont(const std::string&    fontName,
                     const std::string&    fontData,
                     units::pixels<double> size) :
    p(std::make_unique<Impl>(fontName, fontData, size))
{
}
ImGuiFont::~ImGuiFont() = default;

void ImGuiFont::Impl::CreateImGuiFont(const std::string& fontData)
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
      const_cast<void*>(static_cast<const void*>(fontData.c_str())),
      static_cast<int>(std::clamp<std::size_t>(
         fontData.size(), 0, std::numeric_limits<int>::max())),
      sizePixels,
      &fontConfig);
}

ImFont* ImGuiFont::font()
{
   return p->imFont_;
}

} // namespace types
} // namespace qt
} // namespace scwx
