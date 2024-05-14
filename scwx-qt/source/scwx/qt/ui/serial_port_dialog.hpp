#pragma once

#include <QDialog>

namespace Ui
{
class SerialPortDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{
class SerialPortDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(SerialPortDialog)

public:
   explicit SerialPortDialog(QWidget* parent = nullptr);
   ~SerialPortDialog();

   std::string serial_port();

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::SerialPortDialog* ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
