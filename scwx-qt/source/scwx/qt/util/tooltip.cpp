#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/types/font_types.hpp>
#include <scwx/qt/types/text_types.hpp>
#include <scwx/qt/util/imgui.hpp>
#include <scwx/util/logger.hpp>

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
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static std::unique_ptr<QLabel>  tooltipLabel_  = nullptr;
static std::unique_ptr<QWidget> tooltipParent_ = nullptr;

void Initialize()
{
   static bool initialized = false;

   if (initialized)
   {
      return;
   }

   tooltipParent_ = std::make_unique<QWidget>();
   tooltipParent_->setStyleSheet(
      "QToolTip"
      "{"
      "background-color: rgba(15, 15, 15, 191);"
      "border: 1px solid rgba(110, 110, 128, 128);"
      "color: rgba(255, 255, 255, 204);"
      "}");

   tooltipLabel_ = std::make_unique<QLabel>();
   tooltipLabel_->setWindowFlag(Qt::ToolTip);
   tooltipLabel_->setContentsMargins(6, 4, 6, 4);
   tooltipLabel_->setStyleSheet(
      "background-color: rgba(15, 15, 15, 191);"
      "border: 1px solid rgba(110, 110, 128, 128);"
      "color: rgba(255, 255, 255, 204);");

   initialized = true;
}

void Show(const std::string& text, const QPointF& mouseGlobalPos)
{
   Initialize();

   auto& textSettings = settings::TextSettings::Instance();

   std::size_t textWidth =
      static_cast<std::size_t>(textSettings.hover_text_wrap().GetValue());
   types::TooltipMethod tooltipMethod =
      types::GetTooltipMethod(textSettings.tooltip_method().GetValue());

   // Wrap text if enabled
   std::string wrappedText {};
   if (textWidth > 0)
   {
      wrappedText = TextFlow::Column(text).width(textWidth).toString();
   }

   // Display text is either wrapped or unwrapped text (do this to avoid copy
   // when not wrapping)
   const std::string& displayText = (textWidth > 0) ? wrappedText : text;

   if (tooltipMethod == types::TooltipMethod::ImGui)
   {
      util::ImGui::Instance().DrawTooltip(displayText);
   }
   else if (tooltipMethod == types::TooltipMethod::QToolTip)
   {
      static std::size_t id = 0;
      QToolTip::showText(
         mouseGlobalPos.toPoint(),
         QString("<span id='%1' style='font-family:\"%2\"'>%3</span>")
            .arg(++id)
            .arg("Inconsolata")
            .arg(QString::fromStdString(displayText).replace("\n", "<br/>")),
         tooltipParent_.get(),
         {},
         std::numeric_limits<int>::max());
   }
   else if (tooltipMethod == types::TooltipMethod::QLabel)
   {
      // Get monospace font size
      units::font_size::pixels<double> fontSize {16};
      auto                             fontSizes =
         settings::GeneralSettings::Instance().font_sizes().GetValue();
      if (fontSizes.size() > 1)
      {
         fontSize = units::font_size::pixels<double> {fontSizes[1]};
      }
      else if (fontSizes.size() > 0)
      {
         fontSize = units::font_size::pixels<double> {fontSizes[0]};
      }

      // Configure the label
      QFont font("Inconsolata");
      font.setPointSizeF(units::font_size::points<double>(fontSize).value());
      tooltipLabel_->setFont(font);
      tooltipLabel_->setText(QString::fromStdString(displayText));

      // Get the screen the label will be displayed on
      QScreen* screen = QGuiApplication::screenAt(mouseGlobalPos.toPoint());
      if (screen == nullptr)
      {
         screen = QGuiApplication::primaryScreen();
      }

      // Default offset for label
      const QPoint offset {25, 0};

      // Get starting label position (below and to the right)
      QPoint p = mouseGlobalPos.toPoint() + offset;

      // Adjust position if necessary
      const QRect r = screen->geometry();
      if (p.x() + tooltipLabel_->width() > r.x() + r.width())
      {
         // If the label extends beyond the right of the screen, move it left
         p.rx() -= offset.x() * 2 + tooltipLabel_->width();
      }
      if (p.y() + tooltipLabel_->height() > r.y() + r.height())
      {
         // If the label extends beyond the bottom of the screen, move it up
         // p.ry() -= offset.y() * 2 + tooltipLabel_->height();
         // Don't, let it fall through and clamp instead
      }

      // Clamp the label within the screen
      if (p.y() < r.y())
      {
         p.setY(r.y());
      }
      if (p.x() + tooltipLabel_->width() > r.x() + r.width())
      {
         p.setX(r.x() + r.width() - tooltipLabel_->width());
      }
      if (p.x() < r.x())
      {
         p.setX(r.x());
      }
      if (p.y() + tooltipLabel_->height() > r.y() + r.height())
      {
         p.setY(r.y() + r.height() - tooltipLabel_->height());
      }

      // Move the tooltip to the calculated offset
      tooltipLabel_->move(p);

      // Show the tooltip
      if (tooltipLabel_->isHidden())
      {
         tooltipLabel_->show();
      }
   }
}

void Hide()
{
   Initialize();

   // TooltipMethod::QToolTip
   QToolTip::hideText();

   // TooltipMethod::QLabel
   tooltipLabel_->hide();
}

} // namespace tooltip
} // namespace util
} // namespace qt
} // namespace scwx
