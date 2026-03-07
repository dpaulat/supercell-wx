#include "alert_dialog.hpp"
#include "ui_alert_dialog.h"

#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/strings.hpp>
#include <scwx/util/time.hpp>

#include <QPushButton>
#include <QClipboard>
#include <QApplication>
#include <QColor>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::alert_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Phenomenon header background colors
static QColor GetPhenomenonColor(awips::Phenomenon phenomenon)
{
   switch (phenomenon)
   {
   case awips::Phenomenon::Tornado:
      return QColor(0xcc, 0x00, 0x00);
   case awips::Phenomenon::SevereThunderstorm:
      return QColor(0xcc, 0x77, 0x00);
   case awips::Phenomenon::FlashFlood:
      return QColor(0x00, 0x80, 0x00);
   case awips::Phenomenon::Marine:
      return QColor(0x22, 0x55, 0x99);
   case awips::Phenomenon::SnowSquall:
      return QColor(0x00, 0x66, 0x88);
   default:
      return QColor(0x33, 0x33, 0x55);
   }
}

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
   void UpdateCardStyle();

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
   setWindowTitle(tr("Alert Details"));

   // Set monospace font for raw alert text
   QFont monospaceFont("?");
   monospaceFont.setStyleHint(QFont::StyleHint::TypeWriter);
   if (!monospaceFont.fixedPitch())
   {
      monospaceFont.setStyleHint(QFont::StyleHint::Monospace);
   }
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
      [this](const types::TextEventKey& key)
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
           [this]()
           {
              Q_EMIT self_->MoveMap(centroid_.latitude_, centroid_.longitude_);
              self_->close();
           });
}

bool AlertDialog::SelectAlert(const types::TextEventKey& key)
{
   p->key_ = key;

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

   if (messageCount == 0u)
   {
      return;
   }

   bool firstSelected = (currentIndex_ == 0u);
   bool lastSelected  = (currentIndex_ == messageCount - 1u);

   self_->ui->firstButton->setEnabled(!firstSelected);
   self_->ui->previousButton->setEnabled(!firstSelected);
   self_->ui->nextButton->setEnabled(!lastSelected);
   self_->ui->lastButton->setEnabled(!lastSelected);

   self_->ui->messageCountLabel->setText(
      QObject::tr("%1 of %2").arg(currentIndex_ + 1u).arg(messageCount));

   // Get current message and its last segment
   auto& message = messages[currentIndex_];
   if (message->segment_count() == 0)
   {
      return;
   }
   auto segment = message->segments().back();

   // Update centroid / Go button
   if (segment->codedLocation_.has_value())
   {
      centroid_ =
         common::GetCentroid(segment->codedLocation_->coordinates());
   }
   else
   {
      centroid_ = common::Coordinate {};
   }
   goButton_->setEnabled(centroid_ != common::Coordinate {});

   // Phenomenon and significance
   awips::Phenomenon   phenomenon   = key_.phenomenon_;
   awips::Significance significance = key_.significance_;

   QString phenomenonText =
      QString::fromStdString(awips::GetPhenomenonText(phenomenon));
   QString significanceText =
      QString::fromStdString(awips::GetSignificanceText(significance));
   QString alertType = phenomenonText + " " + significanceText;

   // Window title
   self_->setWindowTitle(alertType + " — " +
                         QString::fromStdString(key_.officeId_));

   // Header labels
   self_->ui->alertTypeLabel->setText(alertType.toUpper());
   self_->ui->alertSubtitleLabel->setText(
      QObject::tr("Issued by NWS %1  |  ETN #%2")
         .arg(QString::fromStdString(key_.officeId_))
         .arg(key_.etn_));

   // Header color
   UpdateCardStyle();

   // Threat category
   awips::ibw::ThreatCategory threatCategory = segment->threatCategory_;

   // Emergency = Catastrophic threat
   bool isEmergency =
      (threatCategory == awips::ibw::ThreatCategory::Catastrophic);

   // PDS = Considerable+ threat for Tornado or FlashFlood, but not Emergency
   bool isPds = false;
   if (!isEmergency &&
       threatCategory >= awips::ibw::ThreatCategory::Considerable)
   {
      if (phenomenon == awips::Phenomenon::Tornado ||
          phenomenon == awips::Phenomenon::FlashFlood)
      {
         isPds = true;
      }
   }

   // Badges
   self_->ui->emergencyLabel->setVisible(isEmergency);
   self_->ui->pdsLabel->setVisible(isPds);

   // Observed vs Radar Indicated
   const auto& ibwInfo   = awips::ibw::GetImpactBasedWarningInfo(phenomenon);
   bool        observed  = segment->observed_;
   self_->ui->observedLabel->setVisible(ibwInfo.hasObservedTag_ && observed);
   self_->ui->radarIndicatedLabel->setVisible(ibwInfo.hasObservedTag_ &&
                                              !observed);

   // Tornado Possible
   self_->ui->tornadoPossibleLabel->setVisible(segment->tornadoPossible_);

   // Times
   auto startTime = messages.front()->segment_event_begin(0);
   std::chrono::system_clock::time_point endTime {};
   if (!segment->header_->vtecString_.empty())
   {
      endTime = segment->header_->vtecString_[0].pVtec_.event_end();
   }

   const auto* tz      = scwx::util::time::current_time_zone();
   auto        clockFmt = scwx::util::time::ClockFormat::Default;

   self_->ui->issuedValueLabel->setText(
      QString::fromStdString(scwx::util::TimeString(startTime, clockFmt, tz)));
   self_->ui->expiresValueLabel->setText(
      QString::fromStdString(scwx::util::TimeString(endTime, clockFmt, tz)));

   // Areas affected
   {
      auto&  lastMsg  = messages.back();
      size_t segCount = lastMsg->segment_count();
      auto   lastSeg  = lastMsg->segment(segCount - 1);
      auto   fipsIds  = lastSeg->header_->ugc_.fips_ids();
      auto   states   = lastSeg->header_->ugc_.states();

      std::vector<std::string> counties;
      counties.reserve(fipsIds.size());
      for (auto& id : fipsIds)
      {
         counties.push_back(config::CountyDatabase::GetCountyName(id));
      }
      std::sort(counties.begin(), counties.end());

      QString stateStr =
         QString::fromStdString(scwx::util::ToString(states));
      QString countyStr =
         QString::fromStdString(scwx::util::ToString(counties));

      if (!stateStr.isEmpty() && !countyStr.isEmpty())
      {
         self_->ui->areasValueLabel->setText(stateStr + ": " + countyStr);
      }
      else if (!countyStr.isEmpty())
      {
         self_->ui->areasValueLabel->setText(countyStr);
      }
      else
      {
         self_->ui->areasValueLabel->setText("—");
      }
   }

   // Hazard level
   QString threatName;
   if (isEmergency)
   {
      threatName = "Emergency";
   }
   else if (isPds)
   {
      threatName =
         "PDS — " +
         QString::fromStdString(
            awips::ibw::GetThreatCategoryName(threatCategory));
   }
   else
   {
      threatName = QString::fromStdString(
         awips::ibw::GetThreatCategoryName(threatCategory));
   }
   self_->ui->hazardValueLabel->setText(threatName);

   // Event ID
   self_->ui->eventValueLabel->setText(
      QString("ETN %1 / %2")
         .arg(key_.etn_)
         .arg(QString::fromStdString(key_.officeId_)));

   // Active status badge
   if (segment->header_.has_value() && !segment->header_->vtecString_.empty())
   {
      auto action = segment->header_->vtecString_[0].pVtec_.action();
      bool active = (action != awips::PVtec::Action::Canceled &&
                     action != awips::PVtec::Action::Expired);
      self_->ui->alertStatusLabel->setText(active ? "● ACTIVE" : "○ EXPIRED");
      self_->ui->alertStatusLabel->setStyleSheet(
         active ? "color: #44ff44; font-weight: bold;"
                : "color: #888888; font-weight: bold;");
   }
}

void AlertDialogImpl::UpdateCardStyle()
{
   awips::Phenomenon phenomenon  = key_.phenomenon_;
   QColor            headerColor = GetPhenomenonColor(phenomenon);

   QString headerStyle =
      QString("QFrame#alertHeaderFrame { background-color: %1; }")
         .arg(headerColor.name(QColor::HexRgb));
   self_->ui->alertHeaderFrame->setStyleSheet(headerStyle);

   QString badgesStyle =
      QString("QFrame#alertBadgesFrame { background-color: %1; }")
         .arg(headerColor.darker(170).name(QColor::HexRgb));
   self_->ui->alertBadgesFrame->setStyleSheet(badgesStyle);
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

void AlertDialog::on_screenshotButton_clicked()
{
   // Grab just the alert card frame as a social media card
   QPixmap pixmap = ui->alertCardFrame->grab();

   if (pixmap.isNull())
   {
      logger_->warn("Failed to capture alert card pixmap");
      return;
   }

   // Copy to clipboard immediately
   QApplication::clipboard()->setPixmap(pixmap);

   // Offer to save to a file
   QString defaultName =
      QString("alert_%1_%2.png")
         .arg(QString::fromStdString(p->key_.officeId_))
         .arg(p->key_.etn_);

   QString picturesDir =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

   QString savePath = QFileDialog::getSaveFileName(
      this,
      tr("Save Alert Card"),
      picturesDir + "/" + defaultName,
      tr("PNG Image (*.png);;JPEG Image (*.jpg)"));

   if (!savePath.isEmpty())
   {
      if (!pixmap.save(savePath))
      {
         QMessageBox::warning(this,
                              tr("Save Failed"),
                              tr("Could not save the alert card image."));
      }
   }
}

#include "alert_dialog.moc"

} // namespace ui
} // namespace qt
} // namespace scwx
