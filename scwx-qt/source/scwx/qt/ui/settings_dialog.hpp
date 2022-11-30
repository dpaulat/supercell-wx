#pragma once

#include <QDialog>

namespace Ui
{
class SettingsDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class SettingsDialogImpl;

class SettingsDialog : public QDialog
{
   Q_OBJECT

private:
   Q_DISABLE_COPY(SettingsDialog)

public:
   explicit SettingsDialog(QWidget* parent = nullptr);
   ~SettingsDialog();

private:
   friend SettingsDialogImpl;
   std::unique_ptr<SettingsDialogImpl> p;
   Ui::SettingsDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
