#include "layer_dialog.hpp"
#include "ui_layer_dialog.h"

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
   explicit LayerDialogImpl() {}
   ~LayerDialogImpl() = default;
};

LayerDialog::LayerDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<LayerDialogImpl>()},
    ui(new Ui::LayerDialog)
{
   ui->setupUi(this);
}

LayerDialog::~LayerDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
