#pragma once

#include <QDialog>

namespace Ui
{
class AboutDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class AboutDialogImpl;

class AboutDialog : public QDialog
{
   Q_OBJECT

private:
   Q_DISABLE_COPY(AboutDialog)

public:
   explicit AboutDialog(QWidget* parent = nullptr);
   ~AboutDialog();

private:
   friend AboutDialogImpl;
   std::unique_ptr<AboutDialogImpl> p;
   Ui::AboutDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
