#pragma once

#include <QDialog>

namespace Ui
{
class WFODialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{
class WFODialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(WFODialog)

public:
   explicit WFODialog(QWidget* parent = nullptr);
   ~WFODialog();

   std::string wfo_id();

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::WFODialog*     ui;
};
} // namespace ui
} // namespace qt
} // namespace scwx
