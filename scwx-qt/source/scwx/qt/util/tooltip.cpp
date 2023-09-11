#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/util/imgui.hpp>

#include <TextFlow.hpp>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QToolTip>

namespace scwx
{
namespace qt
{
namespace util
{
namespace tooltip
{

static const std::string logPrefix_ = "scwx::qt::util::tooltip";

enum class TooltipMethod
{
   ImGui,
   QToolTip,
   QLabel
};

static TooltipMethod tooltipMethod_ = TooltipMethod::ImGui;

static std::unique_ptr<QLabel> labelTooltip_ = nullptr;

void Initialize()
{
   static bool initialized = false;

   if (initialized)
   {
      return;
   }

   labelTooltip_ = std::make_unique<QLabel>();
   labelTooltip_->setWindowFlag(Qt::ToolTip);
   labelTooltip_->setMargin(6);
   labelTooltip_->setAttribute(Qt::WidgetAttribute::WA_TranslucentBackground);
   labelTooltip_->setStyleSheet(
      "background-color: rgba(15, 15, 15, 191);"
      "border: 1px solid rgba(110, 110, 128, 128);"
      "color: rgba(255, 255, 255, 204);");

   initialized = true;
}

void Show(const std::string& text, const QPointF& mouseGlobalPos)
{
   Initialize();

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

   if (tooltipMethod_ == TooltipMethod::ImGui)
   {
      util::ImGui::Instance().DrawTooltip(displayText);
   }
   else if (tooltipMethod_ == TooltipMethod::QToolTip)
   {
      static std::size_t id = 0;
      QToolTip::showText(
         mouseGlobalPos.toPoint(),
         QString("<span id='%1' style='font-family:\"%2\"'>%3</span>")
            .arg(++id)
            .arg("Inconsolata")
            .arg(QString::fromStdString(displayText).replace("\n", "<br/>")),
         nullptr,
         {},
         std::numeric_limits<int>::max());
   }
   else if (tooltipMethod_ == TooltipMethod::QLabel)
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

      // Configure the label
      labelTooltip_->setFont(
         QFont("Inconsolata", static_cast<int>(std::round(fontSize * 0.72))));
      labelTooltip_->setText(QString::fromStdString(displayText));

      // Get the screen the label will be displayed on
      QScreen* screen = QGuiApplication::screenAt(mouseGlobalPos.toPoint());
      if (screen == nullptr)
      {
         screen = QGuiApplication::primaryScreen();
      }

      // Default offset for label
      const QPoint offset {2, 24};

      // Get starting label position (below and to the right)
      QPoint p = mouseGlobalPos.toPoint() + offset;

      // Adjust position if necessary
      const QRect r = screen->geometry();
      if (p.x() + labelTooltip_->width() > r.x() + r.width())
      {
         // If the label extends beyond the right of the screen, move it left
         p.rx() -= 4 + labelTooltip_->width();
      }
      if (p.y() + labelTooltip_->height() > r.y() + r.height())
      {
         // If the label extends beyond the bottom of the screen, move it up
         p.ry() -= 24 + labelTooltip_->height();
      }

      // Clamp the label within the screen
      if (p.y() < r.y())
      {
         p.setY(r.y());
      }
      if (p.x() + labelTooltip_->width() > r.x() + r.width())
      {
         p.setX(r.x() + r.width() - labelTooltip_->width());
      }
      if (p.x() < r.x())
      {
         p.setX(r.x());
      }
      if (p.y() + labelTooltip_->height() > r.y() + r.height())
      {
         p.setY(r.y() + r.height() - labelTooltip_->height());
      }

      // Move the label to the calculated offset
      labelTooltip_->move(p);

      // Show the label
      if (labelTooltip_->isHidden())
      {
         labelTooltip_->show();
      }
   }
}

void Hide()
{
   Initialize();

   // TooltipMethod::QToolTip
   QToolTip::hideText();

   // TooltipMethod::QLabel
   labelTooltip_->hide();
}

} // namespace tooltip
} // namespace util
} // namespace qt
} // namespace scwx
