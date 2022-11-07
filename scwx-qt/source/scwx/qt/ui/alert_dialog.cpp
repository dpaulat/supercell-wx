#include "alert_dialog.hpp"
#include "ui_alert_dialog.h"

#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/util/logger.hpp>

#include <QPushButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::alert_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertDialogImpl : public QObject
{
   Q_OBJECT
public:
   explicit AlertDialogImpl(AlertDialog* self) :
       self_ {self},
       textEventManager_ {manager::TextEventManager::Instance()},
       goButton_ {nullptr},
       key_ {},
       centroid_ {},
       currentIndex_ {0u}
   {
   }
   ~AlertDialogImpl() = default;

   void ConnectSignals();
   void SelectIndex(size_t newIndex);
   void UpdateAlertInfo();

   AlertDialog* self_;

   std::shared_ptr<manager::TextEventManager> textEventManager_;

   QPushButton*        goButton_;
   types::TextEventKey key_;
   common::Coordinate  centroid_;
   size_t              currentIndex_;
};

AlertDialog::AlertDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<AlertDialogImpl>(this)},
    ui(new Ui::AlertDialog)
{
   ui->setupUi(this);

   // Set monospace font for alert view
   QFont monospaceFont("?");
   monospaceFont.setStyleHint(QFont::TypeWriter);
   ui->alertText->setFont(monospaceFont);

   // Add Go button to button box
   p->goButton_ = ui->buttonBox->addButton("&Go", QDialogButtonBox::ActionRole);

   p->ConnectSignals();
}

AlertDialog::~AlertDialog()
{
   delete ui;
}

void AlertDialogImpl::ConnectSignals()
{
   connect(
      textEventManager_.get(),
      &manager::TextEventManager::AlertUpdated,
      this,
      [=](const types::TextEventKey& key)
      {
         if (key == key_)
         {
            UpdateAlertInfo();
         }
      },
      Qt::QueuedConnection);
   connect(goButton_,
           &QPushButton::clicked,
           this,
           [=]()
           {
              emit self_->MoveMap(centroid_.latitude_, centroid_.longitude_);
              self_->close();
           });
}

bool AlertDialog::SelectAlert(const types::TextEventKey& key)
{
   p->key_ = key;

   setWindowTitle(QString::fromStdString(key.ToFullString()));

   auto messages = p->textEventManager_->message_list(key);
   if (messages.empty())
   {
      return false;
   }

   p->SelectIndex(messages.size() - 1u);

   return true;
}

void AlertDialogImpl::SelectIndex(size_t newIndex)
{
   size_t messageCount = textEventManager_->message_count(key_);

   if (newIndex >= messageCount)
   {
      return;
   }

   auto messages = textEventManager_->message_list(key_);

   currentIndex_ = newIndex;

   self_->ui->alertText->setText(
      QString::fromStdString(messages[currentIndex_]->message_content()));

   UpdateAlertInfo();
}

void AlertDialogImpl::UpdateAlertInfo()
{
   auto   messages     = textEventManager_->message_list(key_);
   size_t messageCount = messages.size();

   bool firstSelected = (currentIndex_ == 0u);
   bool lastSelected  = (currentIndex_ == messageCount - 1u);

   self_->ui->firstButton->setEnabled(!firstSelected);
   self_->ui->previousButton->setEnabled(!firstSelected);

   self_->ui->nextButton->setEnabled(!lastSelected);
   self_->ui->lastButton->setEnabled(!lastSelected);

   self_->ui->messageCountLabel->setText(
      QObject::tr("%1 of %2").arg(currentIndex_ + 1u).arg(messageCount));

   // Update centroid
   auto alertSegment = messages[currentIndex_]->segments().back();
   if (alertSegment->codedLocation_.has_value())
   {
      centroid_ =
         common::GetCentroid(alertSegment->codedLocation_->coordinates());
   }

   goButton_->setEnabled(centroid_ != common::Coordinate {});
}

void AlertDialog::on_firstButton_clicked()
{
   p->SelectIndex(0);
}

void AlertDialog::on_previousButton_clicked()
{
   p->SelectIndex(p->currentIndex_ - 1u);
}

void AlertDialog::on_nextButton_clicked()
{
   p->SelectIndex(p->currentIndex_ + 1u);
}

void AlertDialog::on_lastButton_clicked()
{
   p->SelectIndex(p->textEventManager_->message_count(p->key_) - 1u);
}

#include "alert_dialog.moc"

} // namespace ui
} // namespace qt
} // namespace scwx
