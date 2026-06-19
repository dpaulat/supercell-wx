#include "animation_dock_widget.hpp"
#include "ui_animation_dock_widget.h"

#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/util/time.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <QTimer>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::animation_dock_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

#if (__cpp_lib_chrono >= 201907L)
using local_days  = std::chrono::local_days;
using zoned_time_ = std::chrono::zoned_time<std::chrono::seconds>;
#else
using local_days  = date::local_days;
using zoned_time_ = date::zoned_time<std::chrono::seconds>;
#endif

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

   local_days           selectedDate_ {};
   std::chrono::seconds selectedTime_ {};

   const scwx::util::time_zone* timeZone_ {nullptr};

   void UpdateTimeZoneLabel(const zoned_time_ zonedTime);
   std::chrono::system_clock::time_point GetTimePoint();
   void SetTimePoint(std::chrono::system_clock::time_point time);

   void ConnectSignals();
   void UpdateAutoUpdateLabel();
};

AnimationDockWidget::AnimationDockWidget(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<AnimationDockWidgetImpl>(this)},
    ui(new Ui::AnimationDockWidget)
{
   ui->setupUi(this);

#if (__cpp_lib_chrono >= 201907L)
   p->timeZone_ = std::chrono::get_tzdb().locate_zone("UTC");
#else
   p->timeZone_ = date::get_tzdb().locate_zone("UTC");
#endif
   const std::chrono::sys_seconds currentTimePoint =
      std::chrono::floor<std::chrono::minutes>(scwx::util::time::now());
   p->SetTimePoint(currentTimePoint);

   // Update maximum date on a timer
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) Qt Owns this memory
   auto* maxDateTimer = new QTimer(this);
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

void AnimationDockWidgetImpl::UpdateTimeZoneLabel(const zoned_time_ zonedTime)
{
#if (__cpp_lib_chrono >= 201907L)
   namespace df                                            = std;
   static constexpr std::string_view kFormatStringTimezone = "{:%Z}";
#else
   namespace df                                   = date;
   static const std::string kFormatStringTimezone = "%Z";
#endif
   const std::string timeZoneStr = df::format(kFormatStringTimezone, zonedTime);
   self_->ui->timeZoneLabel->setText(timeZoneStr.c_str());
}

std::chrono::system_clock::time_point AnimationDockWidgetImpl::GetTimePoint()
{
#if (__cpp_lib_chrono >= 201907L)
   using namespace std::chrono;
#else
   using namespace date;
#endif

   // Convert the local time, to a zoned time, to a system time
   const local_time<std::chrono::seconds> localTime =
      selectedDate_ + selectedTime_;
   const auto zonedTime =
      zoned_time<std::chrono::seconds>(timeZone_, localTime, choose::earliest);
   const std::chrono::sys_seconds systemTime = zonedTime.get_sys_time();

   // This is done to update it when the date changes
   UpdateTimeZoneLabel(zonedTime);

   return systemTime;
}

void AnimationDockWidgetImpl::SetTimePoint(
   std::chrono::system_clock::time_point systemTime)
{
#if (__cpp_lib_chrono >= 201907L)
   using namespace std::chrono;
#else
   using namespace date;
#endif
   // Convert the time to a local time
   auto systemTimeSeconds = time_point_cast<std::chrono::seconds>(systemTime);
   auto zonedTime =
      zoned_time<std::chrono::seconds>(timeZone_, systemTimeSeconds);
   const local_seconds localTime = zonedTime.get_local_time();

   // Get the date and time as seperate fields
   selectedDate_ = floor<days>(localTime);
   selectedTime_ = localTime - selectedDate_;

   // Pull out the local date and time as qt times (with c++20 this could be
   // simplified)
   auto time         = QTime::fromMSecsSinceStartOfDay(static_cast<int>(
      duration_cast<std::chrono::milliseconds>(selectedTime_).count()));
   auto yearMonthDay = year_month_day(selectedDate_);
   auto date         = QDate(int(yearMonthDay.year()),
                     // These are always in a small range, so cast is safe
                     static_cast<int>(unsigned(yearMonthDay.month())),
                     static_cast<int>(unsigned(yearMonthDay.day())));

   // Update labels
   self_->ui->timeEdit->setTime(time);
   self_->ui->dateEdit->setDate(date);

   // Time zone almost certainly just changed, so update it
   UpdateTimeZoneLabel(zonedTime);
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
            selectedDate_ = util::LocalDays(date);
            Q_EMIT self_->DateTimeChanged(GetTimePoint());
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
            selectedTime_ = std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::milliseconds(time.msecsSinceStartOfDay()));
            Q_EMIT self_->DateTimeChanged(GetTimePoint());
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
                    [this](types::Hotkey hotkey, bool isAutoRepeat)
                    {
                       switch (hotkey)
                       {
                       case types::Hotkey::TimelineStepBegin:
                          // Only handle step begin on an initial key press
                          if (!isAutoRepeat)
                          {
                             Q_EMIT self_->AnimationStepBeginSelected();
                          }
                          break;

                       case types::Hotkey::TimelineStepBack:
                          Q_EMIT self_->AnimationStepBackSelected();
                          break;

                       case types::Hotkey::TimelinePlay:
                          // Only handle play on an initial key press
                          if (!isAutoRepeat)
                          {
                             Q_EMIT self_->AnimationPlaySelected();
                          }
                          break;

                       case types::Hotkey::TimelineStepNext:
                          Q_EMIT self_->AnimationStepNextSelected();
                          break;

                       case types::Hotkey::TimelineStepEnd:
                          // Only handle step end on an initial key press
                          if (!isAutoRepeat)
                          {
                             Q_EMIT self_->AnimationStepEndSelected();
                          }
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

void AnimationDockWidget::UpdateTimeZone(const scwx::util::time_zone* timeZone)
{
   // null timezone is really UTC. This simplifies other code.
   if (timeZone == nullptr)
   {
#if (__cpp_lib_chrono >= 201907L)
      timeZone = std::chrono::get_tzdb().locate_zone("UTC");
#else
      timeZone = date::get_tzdb().locate_zone("UTC");
#endif
   }

   // Get the (UTC relative) time that is selected. We want to preserve this
   // across timezone changes.
   auto currentTime = p->GetTimePoint();
   p->timeZone_     = timeZone;
   // Set the (UTC relative) time that was already selected. This ensures that
   // the actual time does not change, only the time zone.
   p->SetTimePoint(currentTime);
}

} // namespace ui
} // namespace qt
} // namespace scwx
