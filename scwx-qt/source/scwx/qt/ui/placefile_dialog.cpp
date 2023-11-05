#include "placefile_dialog.hpp"
#include "ui_placefile_dialog.h"

#include <scwx/qt/ui/placefile_settings_widget.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::placefile_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileDialogImpl
{
public:
   explicit PlacefileDialogImpl() {}
   ~PlacefileDialogImpl() = default;

   PlacefileSettingsWidget* placefileSettingsWidget_ {nullptr};
};

PlacefileDialog::PlacefileDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<PlacefileDialogImpl>()},
    ui(new Ui::PlacefileDialog)
{
   ui->setupUi(this);

   p->placefileSettingsWidget_ = new PlacefileSettingsWidget(this);
   p->placefileSettingsWidget_->layout()->setContentsMargins(0, 0, 0, 0);
   ui->contentsFrame->layout()->addWidget(p->placefileSettingsWidget_);
}

PlacefileDialog::~PlacefileDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
