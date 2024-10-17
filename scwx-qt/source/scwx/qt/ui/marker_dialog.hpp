#pragma once

#include <QDialog>

namespace Ui
{
class MarkerDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class MarkerDialogImpl;

class MarkerDialog : public QDialog
{
   Q_OBJECT

public:
   explicit MarkerDialog(QWidget* parent = nullptr);
   ~MarkerDialog();

private:
   friend class MarkerDialogImpl;
   std::unique_ptr<MarkerDialogImpl> p;
   Ui::MarkerDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
