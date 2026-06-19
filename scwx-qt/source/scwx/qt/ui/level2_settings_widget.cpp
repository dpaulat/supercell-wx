#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/ui/threshold_line_edit_sync.hpp>
#include <scwx/qt/ui/threshold_value_utility.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cmath>
#include <execution>
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
#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::level2_settings_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class Level2SettingsWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit Level2SettingsWidgetImpl(Level2SettingsWidget* self) :
       self_ {self},
       layout_ {new QVBoxLayout(self)},
       elevationButtons_ {},
       elevationCuts_ {}
   {
      // NOLINTBEGIN(cppcoreguidelines-owning-memory) Qt takes care of this
      layout_->setContentsMargins(0, 0, 0, 0);

      incomingElevationLabel_ = new QLabel("", self);
      layout_->addWidget(incomingElevationLabel_);

      elevationGroupBox_ = new QGroupBox(tr("Elevation"), self);
      new ui::FlowLayout(elevationGroupBox_);
      layout_->addWidget(elevationGroupBox_);

      settingsGroupBox_       = new QGroupBox(tr("Settings"), self);
      QLayout* settingsLayout = new QVBoxLayout(settingsGroupBox_);
      layout_->addWidget(settingsGroupBox_);

      declutterCheckBox_ = new QCheckBox(tr("Declutter"), settingsGroupBox_);
      settingsLayout->addWidget(declutterCheckBox_);

      // Threshold controls
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

      settingsGroupBox_->setVisible(false);
      thresholdGroupBox_->setVisible(false);
      // NOLINTEND(cppcoreguidelines-owning-memory) Qt takes care of this

      QObject::connect(hotkeyManager_.get(),
                       &manager::HotkeyManager::HotkeyPressed,
                       this,
                       &Level2SettingsWidgetImpl::HandleHotkeyPressed);

      QObject::connect(thresholdCheckBox_,
                       &QCheckBox::toggled,
                       this,
                       &Level2SettingsWidgetImpl::HandleThresholdToggled);

      QObject::connect(thresholdSlider_,
                       &QSlider::valueChanged,
                       this,
                       &Level2SettingsWidgetImpl::HandleThresholdSliderChanged);

      QObject::connect(thresholdValueEdit_,
                       &QLineEdit::editingFinished,
                       this,
                       &Level2SettingsWidgetImpl::HandleThresholdEditFinished);
   }
   ~Level2SettingsWidgetImpl() = default;

   bool eventFilter(QObject* watched, QEvent* event) override;

   void HandleHotkeyPressed(types::Hotkey hotkey, bool isAutoRepeat);
   void HandleThresholdToggled(bool checked);
   void HandleThresholdSliderChanged(int value);
   void HandleThresholdEditFinished();
   void NormalizeElevationButtons();
   void SelectElevation(float elevation);
   void UpdateThresholdValueDisplay(int  sliderValue,
                                    bool force_line_edit = false);

   [[nodiscard]] float SliderToPhysical(int sliderValue) const;
   [[nodiscard]] int   PhysicalToSlider(float physicalValue) const;

   Level2SettingsWidget* self_;
   QLayout*              layout_;

   QGroupBox*              elevationGroupBox_ {};
   QLabel*                 incomingElevationLabel_ {};
   std::list<QToolButton*> elevationButtons_;
   std::vector<float>      elevationCuts_;
   bool                    elevationButtonsChanged_ {};
   bool                    resizeElevationButtons_ {};

   QGroupBox* settingsGroupBox_ {};
   QCheckBox* declutterCheckBox_ {};

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

   float        currentElevation_ {};
   QToolButton* currentElevationButton_ {nullptr};

   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};
};

Level2SettingsWidget::Level2SettingsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Level2SettingsWidgetImpl>(this)}
{
}

Level2SettingsWidget::~Level2SettingsWidget() {}

bool Level2SettingsWidget::event(QEvent* event)
{
   if (event->type() == QEvent::Type::Paint)
   {
      if (p->elevationButtonsChanged_)
      {
         p->elevationButtonsChanged_ = false;
      }
      else if (p->resizeElevationButtons_)
      {
         p->NormalizeElevationButtons();
      }
   }

   return QWidget::event(event);
}

void Level2SettingsWidget::showEvent(QShowEvent* event)
{
   QWidget::showEvent(event);

   p->NormalizeElevationButtons();
}

void Level2SettingsWidgetImpl::HandleHotkeyPressed(types::Hotkey hotkey,
                                                   bool          isAutoRepeat)
{
   if (hotkey != types::Hotkey::ProductTiltDecrease &&
       hotkey != types::Hotkey::ProductTiltIncrease)
   {
      // Not handling this hotkey
      return;
   }

   logger_->trace("Handling hotkey: {}, repeat: {}",
                  types::GetHotkeyShortName(hotkey),
                  isAutoRepeat);

   if (!self_->isEnabled() || currentElevationButton_ == nullptr)
   {
      // Level 2 product is not selected
      return;
   }

   // Find the current elevation tilt
   auto tiltIt = std::find(elevationButtons_.cbegin(),
                           elevationButtons_.cend(),
                           currentElevationButton_);

   if (tiltIt == elevationButtons_.cend())
   {
      logger_->error("Could not locate level 2 tilt: {}", currentElevation_);
      return;
   }

   if (hotkey == types::Hotkey::ProductTiltDecrease)
   {
      // Validate the current elevation tilt
      if (tiltIt != elevationButtons_.cbegin())
      {
         // Get the previous elevation tilt
         --tiltIt;

         // Select the new elevation tilt
         (*tiltIt)->click();
      }
      else
      {
         logger_->info("Level 2 tilt at lower limit");
      }
   }
   else if (hotkey == types::Hotkey::ProductTiltIncrease)
   {
      // Get the next elevation tilt
      ++tiltIt;

      // Validate the next elevation tilt
      if (tiltIt != elevationButtons_.cend())
      {
         // Select the new elevation tilt
         (*tiltIt)->click();
      }
      else
      {
         logger_->info("Level 2 tilt at upper limit");
      }
   }
}

void Level2SettingsWidgetImpl::NormalizeElevationButtons()
{
   // Set each elevation cut's tool button to the same size
   int elevationCutMaxWidth = 0;
   std::for_each(elevationButtons_.cbegin(),
                 elevationButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->isVisible())
                    {
                       elevationCutMaxWidth =
                          std::max(elevationCutMaxWidth, toolButton->width());
                    }
                 });

   // Don't resize the buttons if the size is out of expected ranges
   if (0 < elevationCutMaxWidth && elevationCutMaxWidth < 100)
   {
      std::for_each(elevationButtons_.cbegin(),
                    elevationButtons_.cend(),
                    [&](auto& toolButton)
                    { toolButton->setMinimumWidth(elevationCutMaxWidth); });

      resizeElevationButtons_ = false;
   }
}

void Level2SettingsWidgetImpl::SelectElevation(float elevation)
{
   self_->UpdateElevationSelection(elevation);

   Q_EMIT self_->ElevationSelected(elevation);
}

bool Level2SettingsWidgetImpl::eventFilter(QObject* watched, QEvent* event)
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

void Level2SettingsWidgetImpl::HandleThresholdToggled(bool checked)
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
      // Emit the current slider value as the threshold
      const float threshold = SliderToPhysical(thresholdSlider_->value());
      Q_EMIT self_->ThresholdChanged(threshold);
   }
   else
   {
      // No threshold
      Q_EMIT self_->ThresholdChanged(std::nullopt);
   }
}

void Level2SettingsWidgetImpl::HandleThresholdSliderChanged(int value)
{
   UpdateThresholdValueDisplay(value, true);

   if (suppressThresholdSignal_ || !thresholdCheckBox_->isChecked())
   {
      return;
   }

   Q_EMIT self_->ThresholdChanged(SliderToPhysical(value));
}

void Level2SettingsWidgetImpl::HandleThresholdEditFinished()
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

void Level2SettingsWidgetImpl::UpdateThresholdValueDisplay(int  sliderValue,
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
      thresholdValueEdit_->setModified(false);
   }
   thresholdUnitsLabel_->setText(
      QString::fromStdString(thresholdUnits_).trimmed());
}

float Level2SettingsWidgetImpl::SliderToPhysical(int sliderValue) const
{
   return ColorTableThresholdSliderToPhysical(sliderValue, thresholdRangeMin_);
}

int Level2SettingsWidgetImpl::PhysicalToSlider(float physicalValue) const
{
   return ColorTableThresholdPhysicalToSlider(physicalValue,
                                              thresholdRangeMin_);
}

void Level2SettingsWidget::UpdateElevationSelection(float elevation)
{
   QString buttonText {QString::number(elevation, 'f', 1) +
                       common::Characters::DEGREE};

   QToolButton* newElevationButton = nullptr;

   std::for_each(p->elevationButtons_.cbegin(),
                 p->elevationButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->text() == buttonText)
                    {
                       newElevationButton = toolButton;
                       toolButton->setCheckable(true);
                       toolButton->setChecked(true);
                    }
                    else
                    {
                       toolButton->setChecked(false);
                       toolButton->setCheckable(false);
                    }
                 });

   p->currentElevation_       = elevation;
   p->currentElevationButton_ = newElevationButton;
}

void Level2SettingsWidget::UpdateIncomingElevation(
   std::optional<float> incomingElevation)
{
   if (incomingElevation.has_value())
   {
      p->incomingElevationLabel_->setText(
         "Incoming Elevation: " + QString::number(*incomingElevation, 'f', 1) +
         common::Characters::DEGREE);
   }
   else
   {
      p->incomingElevationLabel_->setText("Incoming Elevation: None");
   }
}

void Level2SettingsWidget::UpdateSettings(map::MapWidget* activeMap)
{
   std::optional<float> currentElevationOption = activeMap->GetElevation();
   const float          currentElevation =
      currentElevationOption.has_value() ? *currentElevationOption : 0.0f;
   const std::vector<float>   elevationCuts = activeMap->GetElevationCuts();
   const std::optional<float> incomingElevation =
      activeMap->GetIncomingLevel2Elevation();

   if (p->elevationCuts_ != elevationCuts)
   {
      for (auto it = p->elevationButtons_.begin();
           it != p->elevationButtons_.end();)
      {
         delete *it;
         it = p->elevationButtons_.erase(it);
      }

      QLayout* layout = p->elevationGroupBox_->layout();

      // Create elevation cut tool buttons
      for (float elevationCut : elevationCuts)
      {
         QToolButton* toolButton = new QToolButton();
         toolButton->setText(QString::number(elevationCut, 'f', 1) +
                             common::Characters::DEGREE);
         layout->addWidget(toolButton);
         p->elevationButtons_.push_back(toolButton);

         connect(toolButton,
                 &QToolButton::clicked,
                 this,
                 [=, this]() { p->SelectElevation(elevationCut); });
      }

      p->elevationCuts_           = elevationCuts;
      p->elevationButtonsChanged_ = true;
      p->resizeElevationButtons_  = true;
   }

   UpdateElevationSelection(currentElevation);
   UpdateIncomingElevation(incomingElevation);
   UpdateThreshold(activeMap);
}

void Level2SettingsWidget::UpdateThreshold(map::MapWidget* activeMap)
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
      return;
   }

   const int         slider_before  = p->thresholdSlider_->value();
   const bool        checked_before = p->thresholdCheckBox_->isChecked();
   const float       rmin_before    = p->thresholdRangeMin_;
   const float       rmax_before    = p->thresholdRangeMax_;
   const std::string units_before   = p->thresholdUnits_;

   p->suppressThresholdSignal_ = true;

   // Update range if changed
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

      // Set tick interval: approximately 10 units per major tick
      const int tickInterval =
         std::max(1, 10 * kColorTableThresholdSliderStepsPerUnit);
      p->thresholdSlider_->setTickInterval(tickInterval);
   }

   // Update checkbox and slider from current threshold value
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
}

} // namespace ui
} // namespace qt
} // namespace scwx

#include "level2_settings_widget.moc"
