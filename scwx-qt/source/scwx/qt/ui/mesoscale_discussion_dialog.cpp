#include "mesoscale_discussion_dialog.hpp"
#include "ui_mesoscale_discussion_dialog.h"

#include <scwx/qt/manager/spc_md_manager.hpp>
#include <scwx/util/logger.hpp>

#include <QPushButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ =
   "scwx::qt::ui::mesoscale_discussion_dialog";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

class MesoscaleDiscussionDialogImpl : public QObject
{
   Q_OBJECT
public:
   explicit MesoscaleDiscussionDialogImpl(MesoscaleDiscussionDialog* self) :
       self_ {self}, goButton_ {nullptr}, currentIndex_ {0u}
   {
   }
   ~MesoscaleDiscussionDialogImpl() = default;

   void ConnectSignals();
   void SelectIndex(size_t newIndex);
   void UpdateDiscussionInfo();

   MesoscaleDiscussionDialog* self_;

   QPushButton*                                goButton_;
   std::vector<scwx::spc::MesoscaleDiscussion> discussions_;
   scwx::common::Coordinate                    centroid_ {};
   size_t                                      currentIndex_;
};

MesoscaleDiscussionDialog::MesoscaleDiscussionDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<MesoscaleDiscussionDialogImpl>(this)},
    ui(new Ui::MesoscaleDiscussionDialog)
{
   ui->setupUi(this);

   // Add Go button to button box
   p->goButton_ = ui->buttonBox->addButton("&Go", QDialogButtonBox::ActionRole);

   p->ConnectSignals();
}

MesoscaleDiscussionDialog::~MesoscaleDiscussionDialog()
{
   delete ui;
}

void MesoscaleDiscussionDialogImpl::ConnectSignals()
{
   connect(goButton_,
           &QPushButton::clicked,
           this,
           [this]()
           {
              Q_EMIT self_->MoveMap(centroid_.latitude_, centroid_.longitude_);
              self_->close();
           });
}

bool MesoscaleDiscussionDialog::SelectDiscussion(int mdNumber)
{
   auto mdData = manager::SpcMdManager::Instance().GetMdData();
   if (mdData == nullptr || mdData->discussions_.empty())
   {
      logger_->debug("No MD data available");
      return false;
   }

   p->discussions_ = mdData->discussions_;

   // Find the discussion by number
   auto it = std::find_if(p->discussions_.begin(),
                          p->discussions_.end(),
                          [mdNumber](const auto& md)
                          { return md.mdNumber_ == mdNumber; });

   if (it == p->discussions_.end())
   {
      logger_->debug("MD #{} not found in active discussions", mdNumber);
      return false;
   }

   p->currentIndex_ =
      static_cast<size_t>(std::distance(p->discussions_.begin(), it));
   p->SelectIndex(p->currentIndex_);

   return true;
}

void MesoscaleDiscussionDialogImpl::SelectIndex(size_t newIndex)
{
   if (newIndex >= discussions_.size())
   {
      return;
   }

   currentIndex_ = newIndex;

   const auto& md = discussions_[currentIndex_];

   self_->setWindowTitle(QString::fromStdString(
      fmt::format("Mesoscale Discussion #{}", md.mdNumber_)));

   self_->ui->discussionText->setHtml(
      "<pre>" + QString::fromStdString(md.description_) + "</pre>");

   UpdateDiscussionInfo();
}

void MesoscaleDiscussionDialogImpl::UpdateDiscussionInfo()
{
   size_t discussionCount = discussions_.size();

   bool firstSelected = (currentIndex_ == 0u);
   bool lastSelected  = (currentIndex_ == discussionCount - 1u);

   self_->ui->firstButton->setEnabled(!firstSelected);
   self_->ui->previousButton->setEnabled(!firstSelected);

   self_->ui->nextButton->setEnabled(!lastSelected);
   self_->ui->lastButton->setEnabled(!lastSelected);

   self_->ui->discussionCountLabel->setText(
      QObject::tr("%1 of %2").arg(currentIndex_ + 1u).arg(discussionCount));

   centroid_ = discussions_[currentIndex_].centroid_;

   goButton_->setEnabled(centroid_ != scwx::common::Coordinate {});
}

void MesoscaleDiscussionDialog::on_firstButton_clicked()
{
   p->SelectIndex(0);
}

void MesoscaleDiscussionDialog::on_previousButton_clicked()
{
   p->SelectIndex(p->currentIndex_ - 1u);
}

void MesoscaleDiscussionDialog::on_nextButton_clicked()
{
   p->SelectIndex(p->currentIndex_ + 1u);
}

void MesoscaleDiscussionDialog::on_lastButton_clicked()
{
   p->SelectIndex(p->discussions_.size() - 1u);
}

#include "mesoscale_discussion_dialog.moc"

} // namespace ui
} // namespace qt
} // namespace scwx
