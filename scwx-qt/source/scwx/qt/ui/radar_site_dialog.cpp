#include "radar_site_dialog.hpp"
#include "./ui_radar_site_dialog.h"

#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <QPushButton>
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
       radarSiteModel_ {model::RadarSiteModel::Instance()},
       proxyModel_ {new QSortFilterProxyModel(self_)},
       mapPosition_ {},
       mapUpdateDeferred_ {false},
       selectedRadarSite_ {"?"}
   {
      proxyModel_->setSourceModel(radarSiteModel_.get());
      proxyModel_->setSortRole(types::SortRole);
      proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
      proxyModel_->setFilterKeyColumn(-1);
   }
   ~RadarSiteDialogImpl() = default;

   RadarSiteDialog*       self_;
   QSortFilterProxyModel* proxyModel_;

   std::shared_ptr<model::RadarSiteModel> radarSiteModel_;

   scwx::common::Coordinate mapPosition_;
   bool                     mapUpdateDeferred_;

   std::string selectedRadarSite_;
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

   // Button Box
   ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

   connect(ui->radarSiteFilter,
           &QLineEdit::textChanged,
           p->proxyModel_,
           &QSortFilterProxyModel::setFilterWildcard);
   connect(ui->radarSiteView,
           &QAbstractItemView::doubleClicked,
           this,
           [this]() { Q_EMIT accept(); });
   connect(ui->radarSiteView,
           &QAbstractItemView::pressed,
           this,
           [this](const QModelIndex& index)
           {
              QModelIndex selectedIndex = p->proxyModel_->mapToSource(index);

              if (selectedIndex.column() ==
                  static_cast<int>(model::RadarSiteModel::Column::Preset))
              {
                 p->radarSiteModel_->TogglePreset(selectedIndex.row());
              }
           });
   connect(
      ui->radarSiteView->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      this,
      [this](const QItemSelection& selected, const QItemSelection& deselected)
      {
         if (selected.size() == 0 && deselected.size() == 0)
         {
            // Items which stay selected but change their index are not
            // included in selected and deselected. Thus, this signal might
            // be emitted with both selected and deselected empty, if only
            // the indices of selected items change.
            return;
         }

         ui->buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(selected.size() > 0);

         if (selected.size() > 0)
         {
            QModelIndex selectedIndex =
               p->proxyModel_->mapToSource(selected[0].indexes()[0]);
            QVariant variantData = p->radarSiteModel_->data(selectedIndex);
            if (variantData.typeId() == QMetaType::QString)
            {
               p->selectedRadarSite_ = variantData.toString().toStdString();
            }
            else
            {
               logger_->warn("Unexpected selection data type");
               p->selectedRadarSite_ = std::string {"?"};
            }
         }
         else
         {
            p->selectedRadarSite_ = std::string {"?"};
         }

         logger_->debug("Selected: {}", p->selectedRadarSite_);
      });
}

RadarSiteDialog::~RadarSiteDialog()
{
   delete ui;
}

std::string RadarSiteDialog::radar_site() const
{
   return p->selectedRadarSite_;
}

void RadarSiteDialog::showEvent(QShowEvent* event)
{
   if (p->mapUpdateDeferred_)
   {
      p->radarSiteModel_->HandleMapUpdate(p->mapPosition_.latitude_,
                                          p->mapPosition_.longitude_);
      p->mapUpdateDeferred_ = false;
   }

   QDialog::showEvent(event);
}

void RadarSiteDialog::HandleMapUpdate(double latitude, double longitude)
{
   p->mapPosition_ = {latitude, longitude};

   if (isVisible())
   {
      p->radarSiteModel_->HandleMapUpdate(latitude, longitude);
   }
   else
   {
      p->mapUpdateDeferred_ = true;
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
