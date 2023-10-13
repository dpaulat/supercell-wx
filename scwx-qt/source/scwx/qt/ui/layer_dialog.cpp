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
       layerModel_ {new model::LayerModel(self)}
   {
   }
   ~LayerDialogImpl() = default;

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
}

LayerDialog::~LayerDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
