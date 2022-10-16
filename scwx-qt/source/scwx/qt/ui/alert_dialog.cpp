#include "alert_dialog.hpp"
#include "ui_alert_dialog.h"

#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::alert_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertDialogImpl
{
public:
   explicit AlertDialogImpl(AlertDialog* self) :
       self_ {self}, key_ {}, coordinate_ {}, currentIndex_ {0u}
   {
   }
   ~AlertDialogImpl() = default;

   void ConnectSignals();
   void SelectIndex(size_t newIndex);

   AlertDialog*        self_;
   types::TextEventKey key_;
   common::Coordinate  coordinate_;
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
   ui->buttonBox->addButton("&Go", QDialogButtonBox::ActionRole);

   p->ConnectSignals();
}

AlertDialog::~AlertDialog()
{
   delete ui;
}

void AlertDialogImpl::ConnectSignals() {}

bool AlertDialog::SelectAlert(const types::TextEventKey& key,
                              const common::Coordinate&  coordinate)
{
   p->key_        = key;
   p->coordinate_ = coordinate;

   setWindowTitle(QString::fromStdString(key.ToFullString()));

   auto messages = manager::TextEventManager::Instance().message_list(key);
   if (messages.empty())
   {
      return false;
   }

   p->SelectIndex(messages.size() - 1);

   return true;
}

void AlertDialogImpl::SelectIndex(size_t newIndex)
{
   size_t messageCount =
      manager::TextEventManager::Instance().message_count(key_);

   if (newIndex >= messageCount)
   {
      return;
   }

   auto messages = manager::TextEventManager::Instance().message_list(key_);

   currentIndex_ = newIndex;

   self_->ui->alertText->setText(
      QString::fromStdString(messages[currentIndex_]->message_content()));
   self_->ui->messageCountLabel->setText(
      QObject::tr("%1 of %2").arg(currentIndex_ + 1).arg(messageCount));

   bool firstSelected = (currentIndex_ == 0);
   bool lastSelected  = (currentIndex_ == messages.size() - 1);

   self_->ui->firstButton->setEnabled(!firstSelected);
   self_->ui->previousButton->setEnabled(!firstSelected);

   self_->ui->nextButton->setEnabled(!lastSelected);
   self_->ui->lastButton->setEnabled(!lastSelected);
}

} // namespace ui
} // namespace qt
} // namespace scwx
