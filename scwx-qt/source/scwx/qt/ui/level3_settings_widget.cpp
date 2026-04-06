#include <scwx/qt/ui/level3_settings_widget.hpp>
#include <scwx/util/logger.hpp>

#include <cmath>
#include <limits>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

namespace scwx::qt::ui
{

static const std::string logPrefix_ = "scwx::qt::ui::level3_settings_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static constexpr int kSliderStepsPerUnit_ = 10;

class Level3SettingsWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit Level3SettingsWidgetImpl(Level3SettingsWidget* self) :
       self_ {self}, layout_ {new QVBoxLayout(self)}
   {
      // NOLINTBEGIN(cppcoreguidelines-owning-memory) Qt takes care of this
      layout_->setContentsMargins(0, 0, 0, 0);

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
   }
   ~Level3SettingsWidgetImpl() = default;

   void HandleThresholdToggled(bool checked);
   void HandleThresholdSliderChanged(int value);
   void UpdateThresholdLabel(int sliderValue);

   float SliderToPhysical(int sliderValue) const;
   int   PhysicalToSlider(float physicalValue) const;

   Level3SettingsWidget* self_;
   QLayout*              layout_;

   QGroupBox* thresholdGroupBox_ {};
   QCheckBox* thresholdCheckBox_ {};
   QSlider*   thresholdSlider_ {};
   QLabel*    thresholdValueLabel_ {};

   float       thresholdRangeMin_ {-32.0f};
   float       thresholdRangeMax_ {94.5f};
   std::string thresholdUnits_ {};
   bool        suppressThresholdSignal_ {false};
};

Level3SettingsWidget::Level3SettingsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Level3SettingsWidgetImpl>(this)}
{
}

Level3SettingsWidget::~Level3SettingsWidget() {}

void Level3SettingsWidgetImpl::HandleThresholdToggled(bool checked)
{
   thresholdSlider_->setEnabled(checked);

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
   UpdateThresholdLabel(value);

   if (suppressThresholdSignal_ || !thresholdCheckBox_->isChecked())
   {
      return;
   }

   Q_EMIT self_->ThresholdChanged(SliderToPhysical(value));
}

void Level3SettingsWidgetImpl::UpdateThresholdLabel(int sliderValue)
{
   const float physicalValue = SliderToPhysical(sliderValue);
   QString     text          = QString::number(physicalValue, 'f', 1) + " " +
                  QString::fromStdString(thresholdUnits_);
   thresholdValueLabel_->setText(text);
}

float Level3SettingsWidgetImpl::SliderToPhysical(int sliderValue) const
{
   return thresholdRangeMin_ +
          static_cast<float>(sliderValue) / kSliderStepsPerUnit_;
}

int Level3SettingsWidgetImpl::PhysicalToSlider(float physicalValue) const
{
   return static_cast<int>(
      std::round((physicalValue - thresholdRangeMin_) * kSliderStepsPerUnit_));
}

void Level3SettingsWidget::UpdateThreshold(map::MapWidget* activeMap)
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

      const int tickInterval = std::max(1, 10 * kSliderStepsPerUnit_);
      p->thresholdSlider_->setTickInterval(tickInterval);
   }

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

} // namespace scwx::qt::ui

#include "level3_settings_widget.moc"
