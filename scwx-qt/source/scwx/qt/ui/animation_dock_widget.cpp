#include "animation_dock_widget.hpp"
#include "ui_animation_dock_widget.h"

#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/util/time.hpp>
#include <scwx/util/logger.hpp>

#include <QTimer>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::animation_dock_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AnimationDockWidgetImpl
{
public:
   explicit AnimationDockWidgetImpl(AnimationDockWidget* self) : self_ {self}
   {
      static const QString prefix   = QObject::tr("Auto Update");
      static const QString disabled = QObject::tr("Disabled");
      static const QString enabled  = QObject::tr("Enabled");

      enabledString_  = QString("%1: %2").arg(prefix).arg(enabled);
      disabledString_ = QString("%1: %2").arg(prefix).arg(disabled);
   }
   ~AnimationDockWidgetImpl() = default;

   const QIcon kPauseIcon_ {":/res/icons/font-awesome-6/pause-solid.svg"};
   const QIcon kPlayIcon_ {":/res/icons/font-awesome-6/play-solid.svg"};

   QString enabledString_;
   QString disabledString_;

   AnimationDockWidget* self_;

   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};

   types::AnimationState animationState_ {types::AnimationState::Pause};
   types::MapTime        viewType_ {types::MapTime::Live};
   bool                  isLive_ {true};

   std::chrono::sys_days selectedDate_ {};
   std::chrono::seconds  selectedTime_ {};

   void ConnectSignals();
   void UpdateAutoUpdateLabel();
};

AnimationDockWidget::AnimationDockWidget(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<AnimationDockWidgetImpl>(this)},
    ui(new Ui::AnimationDockWidget)
{
   ui->setupUi(this);

   // Set current date/time
   QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
   QDate     currentDate     = currentDateTime.date();
   QTime     currentTime     = currentDateTime.time();
   ui->dateEdit->setDate(currentDate);
   ui->timeEdit->setTime(currentTime);
   ui->dateEdit->setMaximumDate(currentDateTime.date());
   p->selectedDate_ = util::SysDays(currentDate);
   p->selectedTime_ =
      std::chrono::seconds(currentTime.msecsSinceStartOfDay() / 1000);

   // Update maximum date on a timer
   QTimer* maxDateTimer = new QTimer(this);
   connect(maxDateTimer,
           &QTimer::timeout,
           this,
           [this]()
           {
              // Update maximum date to today
              QDate currentDate = QDateTime::currentDateTimeUtc().date();
              if (ui->dateEdit->maximumDate() != currentDate)
              {
                 ui->dateEdit->setMaximumDate(currentDate);
              }
           });

   // Evaluate every 15 seconds, every second is unnecessary
   maxDateTimer->start(15000);

   // Set loop defaults
   auto& generalSettings = settings::GeneralSettings::Instance();
   ui->loopTimeSpinBox->setValue(generalSettings.loop_time().GetValue());
   ui->loopSpeedSpinBox->setValue(generalSettings.loop_speed().GetValue());
   ui->loopDelaySpinBox->setValue(generalSettings.loop_delay().GetValue() *
                                  0.001);

   // Connect widget signals
   p->ConnectSignals();
}

AnimationDockWidget::~AnimationDockWidget()
{
   delete ui;
}

void AnimationDockWidgetImpl::ConnectSignals()
{
   // View type
   QObject::connect(self_->ui->liveViewRadioButton,
                    &QRadioButton::toggled,
                    self_,
                    [this](bool checked)
                    {
                       if (checked)
                       {
                          Q_EMIT self_->ViewTypeChanged(types::MapTime::Live);
                       }
                    });
   QObject::connect(self_->ui->archiveViewRadioButton,
                    &QRadioButton::toggled,
                    self_,
                    [this](bool checked)
                    {
                       if (checked)
                       {
                          Q_EMIT self_->ViewTypeChanged(
                             types::MapTime::Archive);
                       }
                    });

   // Date/time controls
   QObject::connect( //
      self_->ui->dateEdit,
      &QDateTimeEdit::dateChanged,
      self_,
      [this](QDate date)
      {
         if (date.isValid())
         {
            selectedDate_ = util::SysDays(date);
            Q_EMIT self_->DateTimeChanged(selectedDate_ + selectedTime_);
         }
      });
   QObject::connect(
      self_->ui->timeEdit,
      &QDateTimeEdit::timeChanged,
      self_,
      [this](QTime time)
      {
         if (time.isValid())
         {
            selectedTime_ =
               std::chrono::seconds(time.msecsSinceStartOfDay() / 1000);
            Q_EMIT self_->DateTimeChanged(selectedDate_ + selectedTime_);
         }
      });

   // Loop controls
   QObject::connect(
      self_->ui->loopTimeSpinBox,
      &QSpinBox::valueChanged,
      self_,
      [this](int i)
      {
         settings::GeneralSettings::Instance().loop_time().StageValue(i);
         Q_EMIT self_->LoopTimeChanged(std::chrono::minutes(i));
      });
   QObject::connect(
      self_->ui->loopSpeedSpinBox,
      &QDoubleSpinBox::valueChanged,
      self_,
      [this](double d)
      {
         settings::GeneralSettings::Instance().loop_speed().StageValue(d);
         Q_EMIT self_->LoopSpeedChanged(d);
      });
   QObject::connect(
      self_->ui->loopDelaySpinBox,
      &QDoubleSpinBox::valueChanged,
      self_,
      [this](double d)
      {
         settings::GeneralSettings::Instance().loop_delay().StageValue(
            static_cast<std::int64_t>(d * 1000.0));
         Q_EMIT self_->LoopDelayChanged(std::chrono::milliseconds(
            static_cast<typename std::chrono::milliseconds::rep>(d * 1000.0)));
      });

   // Animation controls
   QObject::connect(self_->ui->beginButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { Q_EMIT self_->AnimationStepBeginSelected(); });
   QObject::connect(self_->ui->stepBackButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { Q_EMIT self_->AnimationStepBackSelected(); });
   QObject::connect(self_->ui->playButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { Q_EMIT self_->AnimationPlaySelected(); });
   QObject::connect(self_->ui->stepNextButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { Q_EMIT self_->AnimationStepNextSelected(); });
   QObject::connect(self_->ui->endButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { Q_EMIT self_->AnimationStepEndSelected(); });

   // Shortcuts
   QObject::connect(hotkeyManager_.get(),
                    &manager::HotkeyManager::HotkeyPressed,
                    self_,
                    [this](types::Hotkey hotkey, bool /* isAutoRepeat */)
                    {
                       switch (hotkey)
                       {
                       case types::Hotkey::TimelineStepBegin:
                          Q_EMIT self_->AnimationStepBeginSelected();
                          break;

                       case types::Hotkey::TimelineStepBack:
                          Q_EMIT self_->AnimationStepBackSelected();
                          break;

                       case types::Hotkey::TimelinePlay:
                          Q_EMIT self_->AnimationPlaySelected();
                          break;

                       case types::Hotkey::TimelineStepNext:
                          Q_EMIT self_->AnimationStepNextSelected();
                          break;

                       case types::Hotkey::TimelineStepEnd:
                          Q_EMIT self_->AnimationStepEndSelected();
                          break;

                       default:
                          break;
                       }
                    });
}

void AnimationDockWidget::UpdateAnimationState(types::AnimationState state)
{
   if (p->animationState_ != state)
   {
      // Update icon to opposite of state
      switch (state)
      {
      case types::AnimationState::Pause:
         ui->playButton->setIcon(p->kPlayIcon_);
         break;

      case types::AnimationState::Play:
         ui->playButton->setIcon(p->kPauseIcon_);
         break;
      }

      p->animationState_ = state;
      p->UpdateAutoUpdateLabel();
   }
}

void AnimationDockWidget::UpdateLiveState(bool isLive)
{
   if (p->isLive_ != isLive)
   {
      p->isLive_ = isLive;
      p->UpdateAutoUpdateLabel();
   }
}

void AnimationDockWidget::UpdateViewType(types::MapTime viewType)
{
   if (p->viewType_ != viewType)
   {
      p->viewType_ = viewType;
      p->UpdateAutoUpdateLabel();
   }
}

void AnimationDockWidgetImpl::UpdateAutoUpdateLabel()
{
   // Display "Auto Update: Enabled" if:
   // - The map is live, and auto-updating (map widget update)
   // - "Live" is selected, and the map is playing (timeline manager update)
   if (isLive_ || (viewType_ == types::MapTime::Live &&
                   animationState_ == types::AnimationState::Play))
   {
      self_->ui->autoUpdateLabel->setText(enabledString_);
   }
   else
   {
      self_->ui->autoUpdateLabel->setText(disabledString_);
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
