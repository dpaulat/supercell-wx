#include <qlabel.h>
#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/util/logger.hpp>

#include <cmath>
#include <execution>
#include <limits>

#include <QCheckBox>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
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

static constexpr int kSliderStepsPerUnit_ = 10;

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
      thresholdGroupBox_           = new QGroupBox(tr("Threshold"), self);
      QVBoxLayout* thresholdLayout = new QVBoxLayout(thresholdGroupBox_);

      thresholdCheckBox_ =
         new QCheckBox(tr("Enable Threshold"), thresholdGroupBox_);
      thresholdLayout->addWidget(thresholdCheckBox_);

      QWidget*     sliderWidget = new QWidget(thresholdGroupBox_);
      QHBoxLayout* sliderLayout = new QHBoxLayout(sliderWidget);
      sliderLayout->setContentsMargins(0, 0, 0, 0);

      thresholdSlider_ = new QSlider(Qt::Horizontal, sliderWidget);
      thresholdSlider_->setEnabled(false);
      thresholdSlider_->setTickPosition(QSlider::TicksBelow);
      sliderLayout->addWidget(thresholdSlider_);

      thresholdValueLabel_ = new QLabel("", sliderWidget);
      thresholdValueLabel_->setMinimumWidth(60);
      thresholdValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      sliderLayout->addWidget(thresholdValueLabel_);

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
   }
   ~Level2SettingsWidgetImpl() = default;

   void HandleHotkeyPressed(types::Hotkey hotkey, bool isAutoRepeat);
   void HandleThresholdToggled(bool checked);
   void HandleThresholdSliderChanged(int value);
   void NormalizeElevationButtons();
   void SelectElevation(float elevation);
   void UpdateThresholdLabel(int sliderValue);

   float SliderToPhysical(int sliderValue) const;
   int   PhysicalToSlider(float physicalValue) const;

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
   QLabel*    thresholdValueLabel_ {};

   float       thresholdRangeMin_ {-32.0f};
   float       thresholdRangeMax_ {94.5f};
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

void Level2SettingsWidgetImpl::HandleThresholdToggled(bool checked)
{
   thresholdSlider_->setEnabled(checked);

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
   UpdateThresholdLabel(value);

   if (suppressThresholdSignal_ || !thresholdCheckBox_->isChecked())
   {
      return;
   }

   Q_EMIT self_->ThresholdChanged(SliderToPhysical(value));
}

void Level2SettingsWidgetImpl::UpdateThresholdLabel(int sliderValue)
{
   const float physicalValue = SliderToPhysical(sliderValue);
   QString     text          = QString::number(physicalValue, 'f', 1) + " " +
                  QString::fromStdString(thresholdUnits_);
   thresholdValueLabel_->setText(text);
}

float Level2SettingsWidgetImpl::SliderToPhysical(int sliderValue) const
{
   return thresholdRangeMin_ +
          static_cast<float>(sliderValue) / kSliderStepsPerUnit_;
}

int Level2SettingsWidgetImpl::PhysicalToSlider(float physicalValue) const
{
   return static_cast<int>(
      std::round((physicalValue - thresholdRangeMin_) * kSliderStepsPerUnit_));
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
      return;
   }

   p->suppressThresholdSignal_ = true;

   // Update range if changed
   if (p->thresholdRangeMin_ != rangeMin || p->thresholdRangeMax_ != rangeMax ||
       p->thresholdUnits_ != units)
   {
      p->thresholdRangeMin_ = rangeMin;
      p->thresholdRangeMax_ = rangeMax;
      p->thresholdUnits_    = units;

      const int sliderMin = 0;
      const int sliderMax =
         static_cast<int>((rangeMax - rangeMin) * kSliderStepsPerUnit_);
      p->thresholdSlider_->setRange(sliderMin, sliderMax);

      // Set tick interval: approximately 10 units per major tick
      const int tickInterval = std::max(1, 10 * kSliderStepsPerUnit_);
      p->thresholdSlider_->setTickInterval(tickInterval);
   }

   // Update checkbox and slider from current threshold value
   if (threshold.has_value())
   {
      p->thresholdCheckBox_->setChecked(true);
      p->thresholdSlider_->setValue(p->PhysicalToSlider(*threshold));
   }
   else
   {
      p->thresholdCheckBox_->setChecked(false);
      p->thresholdSlider_->setValue(0);
   }

   p->UpdateThresholdLabel(p->thresholdSlider_->value());
   p->suppressThresholdSignal_ = false;
}

} // namespace ui
} // namespace qt
} // namespace scwx

#include "level2_settings_widget.moc"
