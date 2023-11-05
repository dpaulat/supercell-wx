#pragma once

#include <QDialog>

namespace Ui
{
class OpenUrlDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class OpenUrlDialogImpl;

class OpenUrlDialog : public QDialog
{
   Q_OBJECT

public:
   explicit OpenUrlDialog(const QString& title, QWidget* parent = nullptr);
   ~OpenUrlDialog();

   QString url() const;

protected:
   void showEvent(QShowEvent* event);

private:
   friend class OpenUrlDialogImpl;
   std::unique_ptr<OpenUrlDialogImpl> p;
   Ui::OpenUrlDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
