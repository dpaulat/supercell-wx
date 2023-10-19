#include "layer_dialog.hpp"
#include "ui_layer_dialog.h"

#include <scwx/qt/model/layer_model.hpp>
#include <scwx/util/logger.hpp>

#include <QSortFilterProxyModel>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::layer_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class LayerDialogImpl
{
public:
   explicit LayerDialogImpl(LayerDialog* self) :
       self_ {self},
       layerModel_ {new model::LayerModel(self)},
       layerProxyModel_ {new QSortFilterProxyModel(self_)}
   {
      layerProxyModel_->setSourceModel(layerModel_);
      layerProxyModel_->setFilterCaseSensitivity(
         Qt::CaseSensitivity::CaseInsensitive);
      layerProxyModel_->setFilterKeyColumn(-1);
   }
   ~LayerDialogImpl() = default;

   void ConnectSignals();
   void UpdateMoveButtonsEnabled();

   std::vector<int>              GetSelectedRows();
   std::vector<std::vector<int>> GetContiguousRows();

   LayerDialog*           self_;
   model::LayerModel*     layerModel_;
   QSortFilterProxyModel* layerProxyModel_;
};

LayerDialog::LayerDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<LayerDialogImpl>(this)},
    ui(new Ui::LayerDialog)
{
   ui->setupUi(this);

   ui->layerTreeView->setModel(p->layerProxyModel_);

   auto layerViewHeader = ui->layerTreeView->header();

   layerViewHeader->setMinimumSectionSize(10);

   // Give small columns a fixed size
   for (auto column : model::LayerModel::ColumnIterator())
   {
      if (column != model::LayerModel::Column::Description)
      {
         layerViewHeader->setSectionResizeMode(
            static_cast<int>(column),
            QHeaderView::ResizeMode::ResizeToContents);
      }
   }

   // Disable move buttons
   ui->moveTopButton->setEnabled(false);
   ui->moveUpButton->setEnabled(false);
   ui->moveDownButton->setEnabled(false);
   ui->moveBottomButton->setEnabled(false);

   p->ConnectSignals();
}

LayerDialog::~LayerDialog()
{
   delete ui;
}

void LayerDialogImpl::ConnectSignals()
{
   QObject::connect(self_->ui->layerFilter,
                    &QLineEdit::textChanged,
                    layerProxyModel_,
                    &QSortFilterProxyModel::setFilterWildcard);

   QObject::connect(self_->ui->layerTreeView->selectionModel(),
                    &QItemSelectionModel::selectionChanged,
                    self_,
                    [this](const QItemSelection& /* selected */,
                           const QItemSelection& /* deselected */)
                    { UpdateMoveButtonsEnabled(); });

   QObject::connect(layerModel_,
                    &QAbstractItemModel::rowsMoved,
                    self_,
                    [this]()
                    {
                       UpdateMoveButtonsEnabled();

                       auto selectedRows = GetSelectedRows();
                       if (!selectedRows.empty())
                       {
                          self_->ui->layerTreeView->scrollTo(
                             layerModel_->index(selectedRows.front(), 0));
                       }
                    });

   QObject::connect( //
      self_->ui->moveTopButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         auto contiguousRows   = GetContiguousRows();
         int  destinationChild = 0;

         for (auto& selectedRows : contiguousRows)
         {
            int sourceRow = selectedRows.front();
            int count     = static_cast<int>(selectedRows.size());

            layerModel_->moveRows(QModelIndex(),
                                  sourceRow,
                                  count,
                                  QModelIndex(),
                                  destinationChild);

            // Next set of rows should follow rows just added
            destinationChild += count;
         }
      });
   QObject::connect( //
      self_->ui->moveUpButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         auto contiguousRows   = GetContiguousRows();
         int  destinationChild = -1;

         for (auto& selectedRows : contiguousRows)
         {
            int sourceRow = selectedRows.front();
            int count     = static_cast<int>(selectedRows.size());
            if (destinationChild == -1)
            {
               destinationChild = sourceRow - 1;
            }

            layerModel_->moveRows(QModelIndex(),
                                  sourceRow,
                                  count,
                                  QModelIndex(),
                                  destinationChild);

            // Next set of rows should follow rows just added
            destinationChild += count;
         }
      });
   QObject::connect( //
      self_->ui->moveDownButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         auto contiguousRows   = GetContiguousRows();
         int  destinationChild = 0;
         int  offset           = 0;
         if (!contiguousRows.empty())
         {
            destinationChild = contiguousRows.back().back() + 2;
         }

         for (auto& selectedRows : contiguousRows)
         {
            int sourceRow = selectedRows.front() - offset;
            int count     = static_cast<int>(selectedRows.size());

            layerModel_->moveRows(QModelIndex(),
                                  sourceRow,
                                  count,
                                  QModelIndex(),
                                  destinationChild);

            // Next set of rows should be offset
            offset += count;
         }
      });
   QObject::connect( //
      self_->ui->moveBottomButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         auto contiguousRows   = GetContiguousRows();
         int  destinationChild = layerModel_->rowCount();
         int  offset           = 0;

         for (auto& selectedRows : contiguousRows)
         {
            int sourceRow = selectedRows.front() - offset;
            int count     = static_cast<int>(selectedRows.size());

            layerModel_->moveRows(QModelIndex(),
                                  sourceRow,
                                  count,
                                  QModelIndex(),
                                  destinationChild);

            // Next set of rows should be offset
            offset += count;
         }
      });
}

std::vector<int> LayerDialogImpl::GetSelectedRows()
{
   QModelIndexList selectedRows =
      self_->ui->layerTreeView->selectionModel()->selectedRows();
   std::vector<int> rows {};
   for (auto& selectedRow : selectedRows)
   {
      rows.push_back(layerProxyModel_->mapToSource(selectedRow).row());
   }
   std::sort(rows.begin(), rows.end());
   return rows;
}

std::vector<std::vector<int>> LayerDialogImpl::GetContiguousRows()
{
   std::vector<std::vector<int>> contiguousRows {};
   std::vector<int>              currentContiguousRows {};
   auto                          rows = GetSelectedRows();

   for (auto& row : rows)
   {
      // Next row is not contiguous with current row set
      if (!currentContiguousRows.empty() &&
          currentContiguousRows.back() + 1 < row)
      {
         // Add current row set to contiguous rows, and reset current set
         contiguousRows.emplace_back(std::move(currentContiguousRows));
         currentContiguousRows.clear();
      }

      // Add row to current row set
      currentContiguousRows.push_back(row);
   }

   if (!currentContiguousRows.empty())
   {
      // Add remaining rows to contiguous rows
      contiguousRows.emplace_back(currentContiguousRows);
   }

   return contiguousRows;
}

void LayerDialogImpl::UpdateMoveButtonsEnabled()
{
   QModelIndexList selectedRows =
      self_->ui->layerTreeView->selectionModel()->selectedRows();

   bool itemsSelected    = selectedRows.size() > 0;
   bool itemsMovableUp   = itemsSelected;
   bool itemsMovableDown = itemsSelected;
   int  rowCount         = layerModel_->rowCount();

   for (auto& rowIndex : selectedRows)
   {
      int row = layerProxyModel_->mapToSource(rowIndex).row();
      if (!layerModel_->IsMovable(row))
      {
         // If an item in the selection is not movable, disable all moves
         itemsMovableUp   = false;
         itemsMovableDown = false;
         break;
      }
      else
      {
         // If the first row is selected, items cannot be moved up
         if (row == 0)
         {
            itemsMovableUp = false;
         }

         // If the last row is selected, items cannot be moved down
         if (row == rowCount - 1)
         {
            itemsMovableDown = false;
         }
      }
   }

   // Enable move buttons according to selection
   self_->ui->moveTopButton->setEnabled(itemsMovableUp);
   self_->ui->moveUpButton->setEnabled(itemsMovableUp);
   self_->ui->moveDownButton->setEnabled(itemsMovableDown);
   self_->ui->moveBottomButton->setEnabled(itemsMovableDown);
}

} // namespace ui
} // namespace qt
} // namespace scwx
