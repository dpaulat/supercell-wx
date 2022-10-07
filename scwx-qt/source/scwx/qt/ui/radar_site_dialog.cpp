#include "radar_site_dialog.hpp"
#include "./ui_radar_site_dialog.h"

#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/util/logger.hpp>

#include <QSortFilterProxyModel>

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
   explicit RadarSiteDialogImpl() :
       radarSiteModel_ {nullptr}, proxyModel_ {nullptr}
   {
   }
   ~RadarSiteDialogImpl() = default;

   model::RadarSiteModel* radarSiteModel_;
   QSortFilterProxyModel* proxyModel_;
};

RadarSiteDialog::RadarSiteDialog(QWidget* parent) :
    QDialog(parent),
    p(std::make_unique<RadarSiteDialogImpl>()),
    ui(new Ui::RadarSiteDialog)
{
   ui->setupUi(this);

   p->radarSiteModel_ = new model::RadarSiteModel(this);
   p->proxyModel_     = new QSortFilterProxyModel(this);
   p->proxyModel_->setSourceModel(p->radarSiteModel_);
   ui->radarSiteView->setModel(p->proxyModel_);

   for (int column = 0; column < p->radarSiteModel_->columnCount(); column++)
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
