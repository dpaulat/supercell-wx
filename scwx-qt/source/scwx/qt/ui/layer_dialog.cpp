#include "layer_dialog.hpp"
#include "ui_layer_dialog.h"

#include <scwx/qt/model/layer_model.hpp>
#include <scwx/util/logger.hpp>

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
       self_ {self}, layerModel_ {new model::LayerModel(self)}
   {
   }
   ~LayerDialogImpl() = default;

   void ConnectSignals();

   LayerDialog*       self_;
   model::LayerModel* layerModel_;
};

LayerDialog::LayerDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<LayerDialogImpl>(this)},
    ui(new Ui::LayerDialog)
{
   ui->setupUi(this);

   ui->layerTreeView->setModel(p->layerModel_);

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
   QObject::connect(
      self_->ui->layerTreeView->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      self_,
      [this](const QItemSelection& /* selected */,
             const QItemSelection& /* deselected */)
      {
         QModelIndexList selectedRows =
            self_->ui->layerTreeView->selectionModel()->selectedRows();

         bool itemsSelected    = selectedRows.size() > 0;
         bool itemsMovableUp   = itemsSelected;
         bool itemsMovableDown = itemsSelected;
         int  rowCount         = layerModel_->rowCount();

         for (auto& rowIndex : selectedRows)
         {
            int row = rowIndex.row();
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
      });
}

} // namespace ui
} // namespace qt
} // namespace scwx
