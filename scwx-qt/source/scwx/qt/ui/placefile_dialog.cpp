#include "placefile_dialog.hpp"
#include "ui_placefile_dialog.h"

#include <scwx/qt/manager/outlook_manager.hpp>
#include <scwx/qt/ui/placefile_settings_widget.hpp>
#include <scwx/util/logger.hpp>

#include <QTabWidget>

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

   QTabWidget*              tabWidget_ {nullptr};
   PlacefileSettingsWidget* placefileSettingsWidget_ {nullptr};
   PlacefileSettingsWidget* outlookSettingsWidget_ {nullptr};
};

PlacefileDialog::PlacefileDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<PlacefileDialogImpl>()},
    ui(new Ui::PlacefileDialog)
{
   ui->setupUi(this);

   p->tabWidget_ = new QTabWidget(this);

   // Regular placefiles: everything except manager::OutlookManager's own
   // built-in entries (see below) -- unchanged from before this tab split
   // existed, just excluding what now has its own tab.
   p->placefileSettingsWidget_ = new PlacefileSettingsWidget(
      p->tabWidget_,
      PlacefileSettingsWidget::CategoryMode::ExcludeCategory,
      manager::OutlookPlacefileCategory());
   p->placefileSettingsWidget_->layout()->setContentsMargins(0, 0, 0, 0);
   p->tabWidget_->addTab(p->placefileSettingsWidget_, tr("Placefiles"));

   // Built-in SPC/WPC risk outlooks (manager::OutlookManager) -- same
   // widget, same underlying PlacefileModel/PlacefileManager, just
   // filtered to the opposite side of the same category split.
   p->outlookSettingsWidget_ = new PlacefileSettingsWidget(
      p->tabWidget_,
      PlacefileSettingsWidget::CategoryMode::OnlyCategory,
      manager::OutlookPlacefileCategory());
   p->outlookSettingsWidget_->layout()->setContentsMargins(0, 0, 0, 0);
   p->tabWidget_->addTab(p->outlookSettingsWidget_, tr("Outlooks"));

   ui->contentsFrame->layout()->addWidget(p->tabWidget_);
}

PlacefileDialog::~PlacefileDialog()
{ delete ui; }

} // namespace ui
} // namespace qt
} // namespace scwx
