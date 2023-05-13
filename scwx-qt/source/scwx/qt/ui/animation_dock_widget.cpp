#include "animation_dock_widget.hpp"
#include "ui_animation_dock_widget.h"

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

enum class AnimationState
{
   Play,
   Pause
};

class AnimationDockWidgetImpl
{
public:
   explicit AnimationDockWidgetImpl(AnimationDockWidget* self) : self_ {self} {}
   ~AnimationDockWidgetImpl() = default;

   AnimationDockWidget* self_;

   AnimationState animationState_ {AnimationState::Pause};

   std::chrono::sys_days selectedDate_ {};
   std::chrono::seconds  selectedTime_ {};

   void ConnectSignals();
};

AnimationDockWidget::AnimationDockWidget(QWidget* parent) :
    QDockWidget(parent),
    p {std::make_unique<AnimationDockWidgetImpl>(this)},
    ui(new Ui::AnimationDockWidget)
{
   ui->setupUi(this);

   // Set date/time edit enabled/disabled
   ui->dateEdit->setEnabled(ui->archiveViewRadioButton->isChecked());
   ui->timeEdit->setEnabled(ui->archiveViewRadioButton->isChecked());

   // Update date/time edit enabled/disabled based on Archive View radio button
   connect(ui->archiveViewRadioButton,
           &QRadioButton::toggled,
           this,
           [this](bool checked)
           {
              ui->dateEdit->setEnabled(checked);
              ui->timeEdit->setEnabled(checked);
           });

   // Set current date/time
   QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
   ui->dateEdit->setDate(currentDateTime.date());
   ui->timeEdit->setTime(currentDateTime.time());
   ui->dateEdit->setMaximumDate(currentDateTime.date());

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
                          emit self_->ViewTypeChanged(types::MapTime::Live);
                       }
                    });
   QObject::connect(self_->ui->archiveViewRadioButton,
                    &QRadioButton::toggled,
                    self_,
                    [this](bool checked)
                    {
                       if (checked)
                       {
                          emit self_->ViewTypeChanged(types::MapTime::Archive);
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
            selectedDate_ = date.toStdSysDays();
            emit self_->DateTimeChanged(selectedDate_ + selectedTime_);
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
            emit self_->DateTimeChanged(selectedDate_ + selectedTime_);
         }
      });

   // Loop controls
   QObject::connect(self_->ui->loopTimeSpinBox,
                    &QSpinBox::valueChanged,
                    self_,
                    [this](int i)
                    { emit self_->LoopTimeChanged(std::chrono::minutes(i)); });
   QObject::connect(self_->ui->loopSpeedSpinBox,
                    &QDoubleSpinBox::valueChanged,
                    self_,
                    [this](double d) { emit self_->LoopSpeedChanged(d); });

   // Animation controls
   QObject::connect(self_->ui->beginButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { emit self_->AnimationStepBeginSelected(); });
   QObject::connect(self_->ui->stepBackButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { emit self_->AnimationStepBackSelected(); });
   QObject::connect(self_->ui->playButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]()
                    {
                       if (animationState_ == AnimationState::Pause)
                       {
                          emit self_->AnimationPlaySelected();
                       }
                       else
                       {
                          emit self_->AnimationPauseSelected();
                       }
                    });
   QObject::connect(self_->ui->stepNextButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { emit self_->AnimationStepNextSelected(); });
   QObject::connect(self_->ui->endButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { emit self_->AnimationStepEndSelected(); });
}

} // namespace ui
} // namespace qt
} // namespace scwx
