#include "radar_site_dialog.hpp"
#include "./ui_radar_site_dialog.h"

#include <scwx/qt/model/radar_site_model.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

class RadarSiteDialogImpl
{
public:
   explicit RadarSiteDialogImpl() {}
   ~RadarSiteDialogImpl() = default;
};

RadarSiteDialog::RadarSiteDialog(QWidget* parent) :
    QDialog(parent),
    p(std::make_unique<RadarSiteDialogImpl>()),
    ui(new Ui::RadarSiteDialog)
{
   ui->setupUi(this);

   ui->radarSiteView->setModel(new model::RadarSiteModel(this));
}

RadarSiteDialog::~RadarSiteDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
