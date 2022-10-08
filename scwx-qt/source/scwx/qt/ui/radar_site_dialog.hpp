#pragma once

#include <QDialog>

namespace Ui
{
class RadarSiteDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class RadarSiteDialogImpl;

class RadarSiteDialog : public QDialog
{
   Q_OBJECT

public:
   explicit RadarSiteDialog(QWidget* parent = nullptr);
   ~RadarSiteDialog();

protected:
   void showEvent(QShowEvent*) override;

public slots:
   void HandleMapUpdate(double latitude, double longitude);

private:
   std::unique_ptr<RadarSiteDialogImpl> p;
   Ui::RadarSiteDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
