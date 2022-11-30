#include "settings_dialog.hpp"
#include "ui_settings_dialog.h"

namespace scwx
{
namespace qt
{
namespace ui
{

class SettingsDialogImpl
{
public:
   explicit SettingsDialogImpl() {}
   ~SettingsDialogImpl() = default;
};

SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<SettingsDialogImpl>()},
    ui(new Ui::SettingsDialog)
{
   ui->setupUi(this);
}

SettingsDialog::~SettingsDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
