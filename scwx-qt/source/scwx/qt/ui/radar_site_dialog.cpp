#include "radar_site_dialog.hpp"
#include "./ui_radar_site_dialog.h"

#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::radar_site_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

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

   model::RadarSiteModel* radarSiteModel = new model::RadarSiteModel(this);
   ui->radarSiteView->setModel(radarSiteModel);

   for (int column = 0; column < radarSiteModel->columnCount(); column++)
   {
      ui->radarSiteView->resizeColumnToContents(column);
   }
}

RadarSiteDialog::~RadarSiteDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
