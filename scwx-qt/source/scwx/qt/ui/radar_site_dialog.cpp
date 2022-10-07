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
   explicit RadarSiteDialogImpl(RadarSiteDialog* self) :
       self_ {self},
       radarSiteModel_ {new model::RadarSiteModel(self_)},
       proxyModel_ {new QSortFilterProxyModel(self_)}
   {
      proxyModel_->setSourceModel(radarSiteModel_);
      proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
      proxyModel_->setFilterKeyColumn(-1);
   }
   ~RadarSiteDialogImpl() = default;

   RadarSiteDialog*       self_;
   model::RadarSiteModel* radarSiteModel_;
   QSortFilterProxyModel* proxyModel_;
};

RadarSiteDialog::RadarSiteDialog(QWidget* parent) :
    QDialog(parent),
    p(std::make_unique<RadarSiteDialogImpl>(this)),
    ui(new Ui::RadarSiteDialog)
{
   ui->setupUi(this);

   // Radar Site View
   ui->radarSiteView->setModel(p->proxyModel_);
   ui->radarSiteView->header()->setSortIndicator(0, Qt::AscendingOrder);
   for (int column = 0; column < p->radarSiteModel_->columnCount(); column++)
   {
      ui->radarSiteView->resizeColumnToContents(column);
   }

   connect(ui->radarSiteFilter,
           &QLineEdit::textChanged,
           p->proxyModel_,
           &QSortFilterProxyModel::setFilterWildcard);
}

RadarSiteDialog::~RadarSiteDialog()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
