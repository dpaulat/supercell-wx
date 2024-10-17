#include "marker_dialog.hpp"
#include "ui_marker_dialog.h"

#include <scwx/qt/ui/marker_settings_widget.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::marker_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MarkerDialogImpl
{
public:
   explicit MarkerDialogImpl() {}
   ~MarkerDialogImpl() = default;

   MarkerSettingsWidget* markerSettingsWidget_ {nullptr};
};

MarkerDialog::MarkerDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<MarkerDialogImpl>()},
    ui(new Ui::MarkerDialog)
{
   ui->setupUi(this);

   p->markerSettingsWidget_ = new MarkerSettingsWidget(this);
   p->markerSettingsWidget_->layout()->setContentsMargins(0, 0, 0, 0);
   ui->contentsFrame->layout()->addWidget(p->markerSettingsWidget_);
}

MarkerDialog::~MarkerDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
