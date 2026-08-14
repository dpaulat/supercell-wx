#include "layer_dialog.hpp"
#include "ui_layer_dialog.h"

#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/layer_types.hpp>
#include <scwx/qt/ui/layer_opacity_delegate.hpp>
#include <scwx/qt/ui/widgets/focused_spin_box.hpp>
#include <scwx/util/logger.hpp>

#include <boost/signals2/connection.hpp>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QWidget>

namespace scwx::qt::ui
{

static const std::string logPrefix_ = "scwx::qt::ui::layer_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class LayerDialogImpl
{
public:
   explicit LayerDialogImpl(LayerDialog* self) :
       self_ {self},
       layerModel_ {model::LayerModel::Instance()},
       layerProxyModel_ {new QSortFilterProxyModel(self_)}
   {
      layerProxyModel_->setSourceModel(layerModel_.get());
      layerProxyModel_->setFilterCaseSensitivity(
         Qt::CaseSensitivity::CaseInsensitive);
      layerProxyModel_->setFilterKeyColumn(-1);
   }
   ~LayerDialogImpl() = default;

   void ConnectSignals();
   void UpdateMapDisplayColumns();
   void UpdateMoveButtonsEnabled();
   void UpdateOpacityControls();
   void SetSelectedLayersOpacityPercent(int percent);

   boost::signals2::scoped_connection gridWidthSettingsConnection_ {};
   boost::signals2::scoped_connection gridHeightSettingsConnection_ {};

   std::vector<int>              GetSelectedRows() const;
   std::vector<std::vector<int>> GetContiguousRows() const;

   LayerDialog*                       self_;
   std::shared_ptr<model::LayerModel> layerModel_;
   QSortFilterProxyModel*             layerProxyModel_;
   LayerOpacityDelegate*              opacityDelegate_ {};
   QSlider*                           opacitySlider_ {};
   QFocusedSpinBox*                   opacitySpinBox_ {};
   bool                               updatingOpacityControls_ {false};
};

LayerDialog::LayerDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<LayerDialogImpl>(this)},
    ui(new Ui::LayerDialog)
{
   ui->setupUi(this);

   ui->layerTreeView->setModel(p->layerProxyModel_);

   p->opacityDelegate_ = new LayerOpacityDelegate(ui->layerTreeView);
   ui->layerTreeView->setItemDelegateForColumn(
      static_cast<int>(model::LayerModel::Column::Opacity),
      p->opacityDelegate_);

   auto* opacityFrame  = new QFrame(this);
   auto* opacityLayout = new QHBoxLayout(opacityFrame);
   opacityLayout->setContentsMargins(0, 0, 0, 0);
   opacityLayout->addWidget(new QLabel(tr("Opacity"), opacityFrame));
   p->opacitySlider_ = new QSlider(Qt::Orientation::Horizontal, opacityFrame);
   p->opacitySlider_->setRange(0, 100);
   p->opacitySlider_->setTickPosition(QSlider::TickPosition::TicksBelow);
   p->opacitySlider_->setTickInterval(25);
   p->opacitySlider_->setToolTip(
      tr("Layer opacity. Map style layers stay opaque."));
   opacityLayout->addWidget(p->opacitySlider_);
   p->opacitySpinBox_ = new QFocusedSpinBox(opacityFrame);
   p->opacitySpinBox_->setRange(0, 100);
   p->opacitySpinBox_->setSuffix(tr("%"));
   p->opacitySpinBox_->setKeyboardTracking(false);
   opacityLayout->addWidget(p->opacitySpinBox_);
   ui->verticalLayout->insertWidget(1, opacityFrame);

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

   p->UpdateMapDisplayColumns();

   p->ConnectSignals();
   p->UpdateOpacityControls();
}

LayerDialog::~LayerDialog()
{
   delete ui;
}

void LayerDialog::RefreshMapDisplayColumns()
{
   p->UpdateMapDisplayColumns();
}

void LayerDialogImpl::UpdateMapDisplayColumns()
{
   auto&        generalSettings = settings::GeneralSettings::Instance();
   std::int64_t gridWidth       = generalSettings.grid_width().GetValue();
   std::int64_t gridHeight      = generalSettings.grid_height().GetValue();
   int          mapCount        = static_cast<int>(gridWidth * gridHeight);

   int displayMap1Column =
      static_cast<int>(model::LayerModel::Column::DisplayMap1);

   // For each 0-based map index, 1-n (excluding 0, always displayed)
   for (int mapIndex = 1; mapIndex < static_cast<int>(types::kMapCount_);
        ++mapIndex)
   {
      const int  column = displayMap1Column + mapIndex;
      const bool hide   = mapIndex >= mapCount;
      self_->ui->layerTreeView->setColumnHidden(column, hide);
   }
}

void LayerDialogImpl::ConnectSignals()
{
   QObject::connect(
      self_->ui->buttonBox->button(QDialogButtonBox::StandardButton::Reset),
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         layerModel_->ResetLayers();
         UpdateOpacityControls();
      });

   QObject::connect(self_->ui->layerFilter,
                    &QLineEdit::textChanged,
                    layerProxyModel_,
                    &QSortFilterProxyModel::setFilterWildcard);

   QObject::connect(self_->ui->layerTreeView->selectionModel(),
                    &QItemSelectionModel::selectionChanged,
                    self_,
                    [this](const QItemSelection& /* selected */,
                           const QItemSelection& /* deselected */)
                    {
                       UpdateMoveButtonsEnabled();
                       UpdateOpacityControls();
                    });

   QObject::connect(opacitySlider_,
                    &QSlider::valueChanged,
                    self_,
                    [this](int value)
                    { SetSelectedLayersOpacityPercent(value); });
   QObject::connect(opacitySpinBox_,
                    &QSpinBox::valueChanged,
                    self_,
                    [this](int value)
                    { SetSelectedLayersOpacityPercent(value); });

   QObject::connect(
      layerModel_.get(),
      &QAbstractItemModel::dataChanged,
      self_,
      [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
      {
         const int opacityColumn =
            static_cast<int>(model::LayerModel::Column::Opacity);
         if (topLeft.column() <= opacityColumn &&
             opacityColumn <= bottomRight.column())
         {
            UpdateOpacityControls();
         }
      });
   QObject::connect(layerModel_.get(),
                    &QAbstractItemModel::modelReset,
                    self_,
                    [this]() { UpdateOpacityControls(); });

   auto& generalSettings = settings::GeneralSettings::Instance();
   gridWidthSettingsConnection_ =
      generalSettings.grid_width().changed_signal().connect(
         [this](const auto& /*event*/) { UpdateMapDisplayColumns(); });
   gridHeightSettingsConnection_ =
      generalSettings.grid_height().changed_signal().connect(
         [this](const auto& /*event*/) { UpdateMapDisplayColumns(); });

   QObject::connect(layerModel_.get(),
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

std::vector<int> LayerDialogImpl::GetSelectedRows() const
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

std::vector<std::vector<int>> LayerDialogImpl::GetContiguousRows() const
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

void LayerDialogImpl::UpdateOpacityControls()
{
   updatingOpacityControls_ = true;

   const auto selectedRows   = GetSelectedRows();
   int        opacityPercent = 100;
   bool       anyEditable    = false;

   for (int row : selectedRows)
   {
      const QModelIndex typeIndex = layerModel_->index(
         row, static_cast<int>(model::LayerModel::Column::Type));
      const auto type =
         types::GetLayerType(typeIndex.data(Qt::ItemDataRole::DisplayRole)
                                .toString()
                                .toStdString());
      if (!types::LayerSupportsOpacity(type))
      {
         continue;
      }

      anyEditable                    = true;
      const QModelIndex opacityIndex = layerModel_->index(
         row, static_cast<int>(model::LayerModel::Column::Opacity));
      opacityPercent = opacityIndex.data(Qt::ItemDataRole::EditRole).toInt();
      break;
   }

   opacitySlider_->setEnabled(anyEditable);
   opacitySpinBox_->setEnabled(anyEditable);
   opacitySlider_->setValue(opacityPercent);
   opacitySpinBox_->setValue(opacityPercent);

   updatingOpacityControls_ = false;
}

void LayerDialogImpl::SetSelectedLayersOpacityPercent(int percent)
{
   if (updatingOpacityControls_)
   {
      return;
   }

   updatingOpacityControls_ = true;
   opacitySlider_->setValue(percent);
   opacitySpinBox_->setValue(percent);
   updatingOpacityControls_ = false;

   for (int row : GetSelectedRows())
   {
      const QModelIndex opacityIndex = layerModel_->index(
         row, static_cast<int>(model::LayerModel::Column::Opacity));
      if ((layerModel_->flags(opacityIndex) & Qt::ItemFlag::ItemIsEditable) ==
          Qt::ItemFlag::ItemIsEditable)
      {
         layerModel_->setData(
            opacityIndex, percent, Qt::ItemDataRole::EditRole);
      }
   }
}

} // namespace scwx::qt::ui
