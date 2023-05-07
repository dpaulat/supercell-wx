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

class AnimationDockWidgetImpl
{
public:
   explicit AnimationDockWidgetImpl() = default;
   ~AnimationDockWidgetImpl()         = default;
};

AnimationDockWidget::AnimationDockWidget(QWidget* parent) :
    QDockWidget(parent),
    p {std::make_unique<AnimationDockWidgetImpl>()},
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
}

AnimationDockWidget::~AnimationDockWidget()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
