#pragma once

#include <QDialog>

namespace Ui
{
class PlacefileDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class PlacefileDialogImpl;

class PlacefileDialog : public QDialog
{
   Q_OBJECT

public:
   explicit PlacefileDialog(QWidget* parent = nullptr);
   ~PlacefileDialog();

private:
   friend class PlacefileDialogImpl;
   std::unique_ptr<PlacefileDialogImpl> p;
   Ui::PlacefileDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
