#include <scwx/qt/util/imgui.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/util/logger.hpp>

#include <imgui.h>

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "scwx::qt::util::imgui";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ImGui::Impl
{
public:
   explicit Impl() {}
   ~Impl() {}
};

ImGui::ImGui() : p(std::make_unique<Impl>()) {}
ImGui::~ImGui() = default;

ImGui::ImGui(ImGui&&) noexcept            = default;
ImGui& ImGui::operator=(ImGui&&) noexcept = default;

void ImGui::DrawTooltip(const std::string& hoverText)
{
   auto tooltipFont = manager::FontManager::Instance().GetImGuiFont(
      types::FontCategory::Tooltip);

   ::ImGui::BeginTooltip();
   ::ImGui::PushFont(tooltipFont->font());
   ::ImGui::TextUnformatted(hoverText.c_str());
   ::ImGui::PopFont();
   ::ImGui::EndTooltip();
}

ImGui& ImGui::Instance()
{
   static ImGui instance_ {};
   return instance_;
}

} // namespace util
} // namespace qt
} // namespace scwx
