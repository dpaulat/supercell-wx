#include <scwx/qt/ui/level3_settings_widget.hpp>
#include <scwx/qt/ui/threshold_line_edit_sync.hpp>
#include <scwx/qt/ui/threshold_value_utility.hpp>
#include <scwx/util/logger.hpp>

#include <cmath>
#include <limits>

#include <QCheckBox>
#include <QEvent>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMetaObject>
#include <QSlider>

namespace scwx::qt::ui
{

static const std::string logPrefix_ = "scwx::qt::ui::level3_settings_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class Level3SettingsWidgetImpl : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(Level3SettingsWidgetImpl)

public:
   explicit Level3SettingsWidgetImpl(Level3SettingsWidget* self) :
       self_ {self}, layout_ {new QVBoxLayout(self)}
   {
      // NOLINTBEGIN(cppcoreguidelines-owning-memory) Qt takes care of this
      layout_->setContentsMargins(0, 0, 0, 0);

      thresholdGroupBox_    = new QGroupBox(tr("Threshold"), self);
      auto* thresholdLayout = new QVBoxLayout(thresholdGroupBox_);

      thresholdCheckBox_ =
         new QCheckBox(tr("Enable Threshold"), thresholdGroupBox_);
      thresholdLayout->addWidget(thresholdCheckBox_);

      auto* sliderWidget = new QWidget(thresholdGroupBox_);
      auto* sliderLayout = new QHBoxLayout(sliderWidget);
      sliderLayout->setContentsMargins(0, 0, 0, 0);

      thresholdSlider_ = new QSlider(Qt::Horizontal, sliderWidget);
      thresholdSlider_->setEnabled(false);
      thresholdSlider_->setTickPosition(QSlider::TicksBelow);
      sliderLayout->addWidget(thresholdSlider_);

      thresholdValueEdit_ = new QLineEdit("", sliderWidget);
      {
         const QFontMetrics fm(thresholdValueEdit_->font());
         // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
         constexpr int kFramePad = 8;
         thresholdValueEdit_->setFixedWidth(
            fm.horizontalAdvance(QStringLiteral("-999.9")) + kFramePad);
      }
      thresholdValueEdit_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      thresholdValueEdit_->setEnabled(false);
      sliderLayout->addWidget(thresholdValueEdit_);

      thresholdUnitsLabel_ = new QLabel("", sliderWidget);
      thresholdUnitsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      sliderLayout->addWidget(thresholdUnitsLabel_);

      thresholdValueEdit_->installEventFilter(this);

      thresholdLayout->addWidget(sliderWidget);
      layout_->addWidget(thresholdGroupBox_);

      thresholdGroupBox_->setVisible(false);
      // NOLINTEND(cppcoreguidelines-owning-memory) Qt takes care of this

      QObject::connect(thresholdCheckBox_,
                       &QCheckBox::toggled,
                       this,
                       &Level3SettingsWidgetImpl::HandleThresholdToggled);

      QObject::connect(thresholdSlider_,
                       &QSlider::valueChanged,
                       this,
                       &Level3SettingsWidgetImpl::HandleThresholdSliderChanged);

      QObject::connect(thresholdValueEdit_,
                       &QLineEdit::editingFinished,
                       this,
                       &Level3SettingsWidgetImpl::HandleThresholdEditFinished);
   }
   ~Level3SettingsWidgetImpl() override = default;

   bool eventFilter(QObject* watched, QEvent* event) override;

   void HandleThresholdToggled(bool checked);
   void HandleThresholdSliderChanged(int value);
   void HandleThresholdEditFinished();
   void UpdateThresholdValueDisplay(int  sliderValue,
                                    bool force_line_edit = false);

   [[nodiscard]] float SliderToPhysical(int sliderValue) const;
   [[nodiscard]] int   PhysicalToSlider(float physicalValue) const;

   Level3SettingsWidget* self_;
   QLayout*              layout_;

   QGroupBox* thresholdGroupBox_ {};
   QCheckBox* thresholdCheckBox_ {};
   QSlider*   thresholdSlider_ {};
   QLineEdit* thresholdValueEdit_ {};
   QLabel*    thresholdUnitsLabel_ {};

   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   float thresholdRangeMin_ {-32.0f};
   float thresholdRangeMax_ {94.5f};
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

   std::string thresholdUnits_ {};
   bool        suppressThresholdSignal_ {false};
};

Level3SettingsWidget::Level3SettingsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Level3SettingsWidgetImpl>(this)}
{
}

Level3SettingsWidget::~Level3SettingsWidget() = default;

bool Level3SettingsWidgetImpl::eventFilter(QObject* watched, QEvent* event)
{
   if (watched == thresholdValueEdit_ && event->type() == QEvent::Type::FocusIn)
   {
      QMetaObject::invokeMethod(
         thresholdValueEdit_,
         [this]() { thresholdValueEdit_->selectAll(); },
         Qt::QueuedConnection);
   }
   return QObject::eventFilter(watched, event);
}

void Level3SettingsWidgetImpl::HandleThresholdToggled(bool checked)
{
   thresholdSlider_->setEnabled(checked);
   thresholdValueEdit_->setEnabled(checked);

   UpdateThresholdValueDisplay(thresholdSlider_->value(), true);

   if (suppressThresholdSignal_)
   {
      return;
   }

   if (checked)
   {
      const float threshold = SliderToPhysical(thresholdSlider_->value());
      Q_EMIT self_->ThresholdChanged(threshold);
   }
   else
   {
      Q_EMIT self_->ThresholdChanged(std::nullopt);
   }
}

void Level3SettingsWidgetImpl::HandleThresholdSliderChanged(int value)
{
   UpdateThresholdValueDisplay(value, true);

   if (suppressThresholdSignal_ || !thresholdCheckBox_->isChecked())
   {
      return;
   }

   Q_EMIT self_->ThresholdChanged(SliderToPhysical(value));
}

void Level3SettingsWidgetImpl::HandleThresholdEditFinished()
{
   if (suppressThresholdSignal_)
   {
      UpdateThresholdValueDisplay(thresholdSlider_->value(), true);
      return;
   }

   if (!thresholdCheckBox_->isChecked())
   {
      UpdateThresholdValueDisplay(thresholdSlider_->value(), true);
      return;
   }

   bool          ok = false;
   const QString t  = thresholdValueEdit_->text().trimmed();
   const float   v  = QLocale::system().toFloat(t, &ok);
   if (!ok || !std::isfinite(v))
   {
      UpdateThresholdValueDisplay(thresholdSlider_->value(), true);
      return;
   }

   const float clamped = std::clamp(v, thresholdRangeMin_, thresholdRangeMax_);
   const int   newSlider = PhysicalToSlider(clamped);

   if (newSlider != thresholdSlider_->value())
   {
      thresholdSlider_->setValue(newSlider);
   }
   else
   {
      UpdateThresholdValueDisplay(thresholdSlider_->value(), true);
   }
}

void Level3SettingsWidgetImpl::UpdateThresholdValueDisplay(int  sliderValue,
                                                           bool force_line_edit)
{
   const float   physicalValue = SliderToPhysical(sliderValue);
   const QString text = QLocale::system().toString(physicalValue, 'f', 1);
   if (ShouldApplyThresholdLineEditText(*thresholdValueEdit_,
                                        sliderValue,
                                        thresholdRangeMin_,
                                        thresholdRangeMax_,
                                        force_line_edit))
   {
      thresholdValueEdit_->setText(text);
   }
   thresholdUnitsLabel_->setText(
      QString::fromStdString(thresholdUnits_).trimmed());
}

float Level3SettingsWidgetImpl::SliderToPhysical(int sliderValue) const
{
   return ColorTableThresholdSliderToPhysical(sliderValue, thresholdRangeMin_);
}

int Level3SettingsWidgetImpl::PhysicalToSlider(float physicalValue) const
{
   return ColorTableThresholdPhysicalToSlider(physicalValue,
                                              thresholdRangeMin_);
}

bool Level3SettingsWidget::UpdateThreshold(map::MapWidget* activeMap)
{
   const auto [rangeMin, rangeMax]      = activeMap->GetColorTableRange();
   const std::string          units     = activeMap->GetColorTableUnits();
   const std::optional<float> threshold = activeMap->GetColorTableThreshold();

   const bool validRange =
      std::isfinite(rangeMin) && std::isfinite(rangeMax) && rangeMin < rangeMax;

   p->thresholdGroupBox_->setVisible(validRange);

   if (!validRange)
   {
      p->suppressThresholdSignal_ = true;
      if (std::isfinite(rangeMin) && std::isfinite(rangeMax))
      {
         p->thresholdRangeMin_ = rangeMin;
         p->thresholdRangeMax_ = rangeMax;
         p->thresholdUnits_    = units;
      }
      if (threshold.has_value())
      {
         p->thresholdCheckBox_->setChecked(false);
         p->thresholdSlider_->setValue(0);

         activeMap->SetColorTableThreshold(std::nullopt);
      }
      const bool force_line_edit =
         threshold.has_value() ||
         (std::isfinite(rangeMin) && std::isfinite(rangeMax));
      p->UpdateThresholdValueDisplay(p->thresholdSlider_->value(),
                                     force_line_edit);
      p->suppressThresholdSignal_ = false;
      return false;
   }

   const int         slider_before  = p->thresholdSlider_->value();
   const bool        checked_before = p->thresholdCheckBox_->isChecked();
   const float       rmin_before    = p->thresholdRangeMin_;
   const float       rmax_before    = p->thresholdRangeMax_;
   const std::string units_before   = p->thresholdUnits_;

   p->suppressThresholdSignal_ = true;

   if (p->thresholdRangeMin_ != rangeMin || p->thresholdRangeMax_ != rangeMax ||
       p->thresholdUnits_ != units)
   {
      p->thresholdRangeMin_ = rangeMin;
      p->thresholdRangeMax_ = rangeMax;
      p->thresholdUnits_    = units;

      const int sliderMin = 0;
      const int sliderMax = static_cast<int>(
         (rangeMax - rangeMin) *
         static_cast<float>(kColorTableThresholdSliderStepsPerUnit));
      p->thresholdSlider_->setRange(sliderMin, sliderMax);

      const int tickInterval =
         std::max(1, 10 * kColorTableThresholdSliderStepsPerUnit);
      p->thresholdSlider_->setTickInterval(tickInterval);
   }

   if (threshold.has_value())
   {
      const float clampedThreshold = std::clamp(*threshold, rangeMin, rangeMax);
      p->thresholdCheckBox_->setChecked(true);
      p->thresholdSlider_->setValue(p->PhysicalToSlider(clampedThreshold));

      // If threshold was clamped, sync the map's threshold to the clamped value
      if (clampedThreshold != *threshold)
      {
         activeMap->SetColorTableThreshold(clampedThreshold);
      }
   }
   else
   {
      p->thresholdCheckBox_->setChecked(false);
      p->thresholdSlider_->setValue(0);
   }

   const bool range_or_units_changed =
      (rmin_before != rangeMin || rmax_before != rangeMax ||
       units_before != units);
   const bool slider_changed = slider_before != p->thresholdSlider_->value();
   const bool checkbox_changed =
      checked_before != p->thresholdCheckBox_->isChecked();
   const bool force_line_edit =
      range_or_units_changed || slider_changed || checkbox_changed;
   p->UpdateThresholdValueDisplay(p->thresholdSlider_->value(),
                                  force_line_edit);
   p->suppressThresholdSignal_ = false;
   return true;
}

} // namespace scwx::qt::ui

#include "level3_settings_widget.moc"
