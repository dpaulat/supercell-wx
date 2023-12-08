#include "county_dialog.hpp"
#include "ui_county_dialog.h"

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

static const std::string logPrefix_ = "scwx::qt::ui::county_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class CountyDialog::Impl
{
public:
   explicit Impl(CountyDialog* self) :
       self_ {self},
       model_ {new QStandardItemModel(self)},
       proxyModel_ {new QSortFilterProxyModel(self)},
       states_ {config::CountyDatabase::GetStates()}
   {
   }
   ~Impl() = default;

   void UpdateModel(const std::string& stateName);

   CountyDialog*          self_;
   QStandardItemModel*    model_;
   QSortFilterProxyModel* proxyModel_;

   std::string selectedCounty_ {"?"};

   const std::unordered_map<std::string, std::string>& states_;
};

CountyDialog::CountyDialog(QWidget* parent) :
    QDialog(parent), p {std::make_unique<Impl>(this)}, ui(new Ui::CountyDialog)
{
   ui->setupUi(this);

   for (auto& state : p->states_)
   {
      ui->stateComboBox->addItem(QString::fromStdString(state.second));
   }
   ui->stateComboBox->model()->sort(0);
   ui->stateComboBox->setCurrentIndex(0);

   p->proxyModel_->setSourceModel(p->model_);
   ui->countyView->setModel(p->proxyModel_);
   ui->countyView->setEditTriggers(
      QAbstractItemView::EditTrigger::NoEditTriggers);
   ui->countyView->sortByColumn(0, Qt::SortOrder::AscendingOrder);
   ui->countyView->header()->setSectionResizeMode(
      QHeaderView::ResizeMode::Stretch);

   connect(ui->stateComboBox,
           &QComboBox::currentTextChanged,
           this,
           [this](const QString& text) { p->UpdateModel(text.toStdString()); });
   p->UpdateModel(ui->stateComboBox->currentText().toStdString());

   // Button Box
   ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   connect(ui->countyView,
           &QTreeView::doubleClicked,
           this,
           [this]() { Q_EMIT accept(); });
   connect(
      ui->countyView->selectionModel(),
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
               p->selectedCounty_ = variantData.toString().toStdString();
            }
            else
            {
               logger_->warn("Unexpected selection data type");
               p->selectedCounty_ = std::string {"?"};
            }
         }
         else
         {
            p->selectedCounty_ = std::string {"?"};
         }

         logger_->debug("Selected: {}", p->selectedCounty_);
      });
}

CountyDialog::~CountyDialog()
{
   delete ui;
}

std::string CountyDialog::county_fips_id()
{
   return p->selectedCounty_;
}

void CountyDialog::SelectState(const std::string& state)
{
   auto it = p->states_.find(state);
   if (it != p->states_.cend())
   {
      ui->stateComboBox->setCurrentText(QString::fromStdString(it->second));
   }
}

void CountyDialog::Impl::UpdateModel(const std::string& stateName)
{
   // Clear existing counties
   model_->clear();

   // Reset selected county and disable OK button
   selectedCounty_ = std::string {"?"};
   self_->ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   // Reset headers
   model_->setHorizontalHeaderLabels({tr("County / Area"), tr("FIPS ID")});

   // Find the state ID from the statename
   auto it = std::find_if(states_.cbegin(),
                          states_.cend(),
                          [&](const std::pair<std::string, std::string>& record)
                          { return record.second == stateName; });

   if (it != states_.cend())
   {
      QStandardItem* root = model_->invisibleRootItem();

      // Add each county to the model
      for (auto& county : config::CountyDatabase::GetCounties(it->first))
      {
         root->appendRow(
            {new QStandardItem(QString::fromStdString(county.second)),
             new QStandardItem(QString::fromStdString(county.first))});
      }
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
