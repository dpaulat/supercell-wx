#include <scwx/qt/ui/widgets/imgui_button.hpp>
#include <scwx/qt/util/color.hpp>

#include <fmt/format.h>

namespace scwx::qt::ui
{

class ImGuiButton::Impl
{
public:
   explicit Impl(ImGuiButton* self) : self_ {self} {};
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void UpdateButton(const settings::ButtonSettings& buttonSettings);

   ImGuiButton* self_;

   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   boost::gil::rgba8_pixel_t borderColor_ {0, 0, 0, 255};
   boost::gil::rgba8_pixel_t highlightColor_ {255, 255, 0, 255};
   boost::gil::rgba8_pixel_t lineColor_ {0, 0, 255, 255};
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

   boost::signals2::scoped_connection settingsStaged_ {};
};

ImGuiButton::ImGuiButton(QWidget* parent) :
    QPushButton(parent), p {std::make_unique<Impl>(this)}
{
}
ImGuiButton::~ImGuiButton() = default;

void ImGuiButton::set_button_settings(settings::ButtonSettings& buttonSettings)
{
   p->settingsStaged_ = buttonSettings.staged_signal().connect(
      [this, &buttonSettings]() { p->UpdateButton(buttonSettings); });

   p->UpdateButton(buttonSettings);
}

void ImGuiButton::SetStyle(const boost::gil::rgba8_pixel_t& buttonColor,
                           const boost::gil::rgba8_pixel_t& hoverColor,
                           const boost::gil::rgba8_pixel_t& activeColor)
{
   // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
   static const boost::gil::rgba8_pixel_t kBlack {0, 0, 0, 255};

   setStyleSheet(QString::fromStdString(fmt::format(
      "QPushButton {{"
      "  border: 1px solid #806e6e80;"
      "  background-color: {};"
      "  color: #fcffffff;"
      "  padding: 4px 3px;"
      "}}"
      "QPushButton:hover {{"
      "  background-color: {};"
      "}}"
      "QPushButton:pressed {{"
      "  background-color: {};"
      "}}",
      util::color::ToArgbString(util::color::Blend(buttonColor, kBlack)),
      util::color::ToArgbString(util::color::Blend(hoverColor, kBlack)),
      util::color::ToArgbString(util::color::Blend(activeColor, kBlack)))));
}

void ImGuiButton::Impl::UpdateButton(
   const settings::ButtonSettings& buttonSettings)
{
   auto buttonColor = util::color::ToRgba8PixelT(
      buttonSettings.button_color().GetStagedOrValue());
   auto hoverColor = util::color::ToRgba8PixelT(
      buttonSettings.hover_color().GetStagedOrValue());
   auto activeColor = util::color::ToRgba8PixelT(
      buttonSettings.active_color().GetStagedOrValue());

   self_->SetStyle(buttonColor, hoverColor, activeColor);
}

} // namespace scwx::qt::ui
