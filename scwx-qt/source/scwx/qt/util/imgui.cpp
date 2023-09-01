#include <scwx/qt/util/imgui.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/util/logger.hpp>

#include <mutex>

#include <TextFlow.hpp>
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

   void Initialize();
   void UpdateMonospaceFont();

   bool initialized_ {false};

   ImFont* monospaceFont_ {nullptr};
};

ImGui::ImGui() : p(std::make_unique<Impl>()) {}
ImGui::~ImGui() = default;

ImGui::ImGui(ImGui&&) noexcept            = default;
ImGui& ImGui::operator=(ImGui&&) noexcept = default;

void ImGui::Impl::Initialize()
{
   if (initialized_)
   {
      return;
   }

   logger_->debug("Initialize");

   // Configure monospace font
   UpdateMonospaceFont();
   manager::SettingsManager::general_settings()
      .font_sizes()
      .RegisterValueChangedCallback([this](const std::vector<std::int64_t>&)
                                    { UpdateMonospaceFont(); });

   initialized_ = true;
}

void ImGui::Impl::UpdateMonospaceFont()
{
   // Get monospace font size
   std::size_t fontSize = 16;
   auto        fontSizes =
      manager::SettingsManager::general_settings().font_sizes().GetValue();
   if (fontSizes.size() > 1)
   {
      fontSize = fontSizes[1];
   }
   else if (fontSizes.size() > 0)
   {
      fontSize = fontSizes[0];
   }

   // Get monospace font pointer
   auto monospace =
      manager::ResourceManager::Font(types::Font::Inconsolata_Regular);
   auto monospaceFont = monospace->ImGuiFont(fontSize);

   // Store monospace font pointer if not null
   if (monospaceFont != nullptr)
   {
      monospaceFont_ = monospace->ImGuiFont(fontSize);
   }
}

void ImGui::DrawTooltip(const std::string& hoverText)
{
   p->Initialize();

   std::size_t textWidth = static_cast<std::size_t>(
      settings::TextSettings::Instance().hover_text_wrap().GetValue());

   // Wrap text if enabled
   std::string wrappedText {};
   if (textWidth > 0)
   {
      wrappedText = TextFlow::Column(hoverText).width(textWidth).toString();
   }

   // Display text is either wrapped or unwrapped text (do this to avoid copy
   // when not wrapping)
   const std::string& displayText = (textWidth > 0) ? wrappedText : hoverText;

   ::ImGui::BeginTooltip();
   ::ImGui::PushFont(p->monospaceFont_);
   ::ImGui::TextUnformatted(displayText.c_str());
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
