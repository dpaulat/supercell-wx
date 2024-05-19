#pragma once

#include <QDialog>

namespace Ui
{
class GpsInfoDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class GpsInfoDialog : public QDialog
{
   Q_OBJECT

private:
   Q_DISABLE_COPY(GpsInfoDialog)

public:
   explicit GpsInfoDialog(QWidget* parent = nullptr);
   ~GpsInfoDialog();

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::GpsInfoDialog*    ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
