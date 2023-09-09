#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/util/imgui.hpp>

#include <TextFlow.hpp>

namespace scwx
{
namespace qt
{
namespace util
{
namespace tooltip
{

static const std::string logPrefix_ = "scwx::qt::util::tooltip";

void Show(const std::string& text, const QPointF& /* mouseGlobalPos */)
{
   std::size_t textWidth = static_cast<std::size_t>(
      settings::TextSettings::Instance().hover_text_wrap().GetValue());

   // Wrap text if enabled
   std::string wrappedText {};
   if (textWidth > 0)
   {
      wrappedText = TextFlow::Column(text).width(textWidth).toString();
   }

   // Display text is either wrapped or unwrapped text (do this to avoid copy
   // when not wrapping)
   const std::string& displayText = (textWidth > 0) ? wrappedText : text;

   util::ImGui::Instance().DrawTooltip(displayText);
}

void Hide() {}

} // namespace tooltip
} // namespace util
} // namespace qt
} // namespace scwx
