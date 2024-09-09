#include "wfo_dialog.hpp"
#include "ui_wfo_dialog.h"

#include <scwx/qt/config/county_database.hpp>
#include <scwx/util/logger.hpp>

#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::wfo_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class WFODialog::Impl
{
public:
   explicit Impl(WFODialog* self) :
       self_ {self},
       model_ {new QStandardItemModel(self)},
       proxyModel_ {new QSortFilterProxyModel(self)}
   {
   }
   ~Impl() = default;

   void UpdateModel();

   WFODialog*             self_;
   QStandardItemModel*    model_;
   QSortFilterProxyModel* proxyModel_;

   std::string selectedWFO_ {"?"};
};

WFODialog::WFODialog(QWidget* parent) :
    QDialog(parent), p {std::make_unique<Impl>(this)}, ui(new Ui::WFODialog)
{
   ui->setupUi(this);

   p->proxyModel_->setSourceModel(p->model_);
   ui->wfoView->setModel(p->proxyModel_);
   ui->wfoView->setEditTriggers(
      QAbstractItemView::EditTrigger::NoEditTriggers);
   ui->wfoView->sortByColumn(0, Qt::SortOrder::AscendingOrder);
   ui->wfoView->header()->setSectionResizeMode(
      QHeaderView::ResizeMode::Stretch);

   p->UpdateModel();

   // Button Box
   ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   connect(ui->wfoView,
           &QTreeView::doubleClicked,
           this,
           [this]() { Q_EMIT accept(); });
   connect(
      ui->wfoView->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      this,
      [this](const QItemSelection& selected, const QItemSelection& deselected)
      {
         if (selected.size() == 0 && deselected.size() == 0)
         {
            // Items which stay selected but change their index are not
            // included in selected and deselected. Thus, this signal might
            // be emitted with both selected and deselected empty, if only
            // the indices of selected items change.
            return;
         }

         ui->buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(selected.size() > 0);

         if (selected.size() > 0)
         {
            QModelIndex selectedIndex =
               p->proxyModel_->mapToSource(selected[0].indexes()[0]);
            selectedIndex        = p->model_->index(selectedIndex.row(), 1);
            QVariant variantData = p->model_->data(selectedIndex);
            if (variantData.typeId() == QMetaType::QString)
            {
               p->selectedWFO_ = variantData.toString().toStdString();
            }
            else
            {
               logger_->warn("Unexpected selection data type");
               p->selectedWFO_ = std::string {"?"};
            }
         }
         else
         {
            p->selectedWFO_ = std::string {"?"};
         }

         logger_->debug("Selected: {}", p->selectedWFO_);
      });
}

WFODialog::~WFODialog()
{
   delete ui;
}

std::string WFODialog::wfo_id()
{
   return p->selectedWFO_;
}

void WFODialog::Impl::UpdateModel()
{
   // Clear existing counties
   model_->clear();

   // Disable OK button
   self_->ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   // Reset headers
   model_->setHorizontalHeaderLabels({tr("State and City"), tr("ID")});

   QStandardItem* root = model_->invisibleRootItem();

   // Add each wfo to the model
   for (auto& wfo : config::CountyDatabase::GetWFOs())
   {
      root->appendRow(
         {new QStandardItem(QString::fromStdString(wfo.second)),
          new QStandardItem(QString::fromStdString(wfo.first))});
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
