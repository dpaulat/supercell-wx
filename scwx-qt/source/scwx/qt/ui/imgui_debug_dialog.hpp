#pragma once

#include <QDialog>

namespace Ui
{
class ImGuiDebugDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class ImGuiDebugDialogImpl;

class ImGuiDebugDialog : public QDialog
{
private:
   Q_DISABLE_COPY(ImGuiDebugDialog)

public:
   explicit ImGuiDebugDialog(QWidget* parent = nullptr);
   ~ImGuiDebugDialog();

private:
   friend class ImGuiDebugDialogImpl;
   std::unique_ptr<ImGuiDebugDialogImpl> p;
   Ui::ImGuiDebugDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
