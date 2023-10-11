#pragma once

#include <QDialog>

namespace Ui
{
class LayerDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class LayerDialogImpl;

class LayerDialog : public QDialog
{
   Q_OBJECT

public:
   explicit LayerDialog(QWidget* parent = nullptr);
   ~LayerDialog();

private:
   friend class LayerDialogImpl;
   std::unique_ptr<LayerDialogImpl> p;
   Ui::LayerDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
