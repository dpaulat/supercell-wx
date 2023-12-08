#pragma once

#include <QDialog>

namespace Ui
{
class CountyDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{
class CountyDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(CountyDialog)

public:
   explicit CountyDialog(QWidget* parent = nullptr);
   ~CountyDialog();

   std::string county_fips_id();

   void SelectState(const std::string& state);

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::CountyDialog*     ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
