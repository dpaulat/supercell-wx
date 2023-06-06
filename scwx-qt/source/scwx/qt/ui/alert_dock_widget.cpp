#include "alert_dock_widget.hpp"
#include "ui_alert_dock_widget.h"

#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/model/alert_proxy_model.hpp>
#include <scwx/qt/types/alert_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/ui/alert_dialog.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::alert_dock_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertDockWidgetImpl : QObject
{
   Q_OBJECT
public:
   explicit AlertDockWidgetImpl(AlertDockWidget* self) :
       self_ {self},
       textEventManager_ {manager::TextEventManager::Instance()},
       alertModel_ {std::make_unique<model::AlertModel>()},
       proxyModel_ {std::make_unique<model::AlertProxyModel>()},
       alertDialog_ {new AlertDialog(self)},
       mapPosition_ {},
       mapUpdateDeferred_ {false},
       selectedAlertKey_ {},
       selectedAlertCentroid_ {}
   {
      proxyModel_->setSourceModel(alertModel_.get());
      proxyModel_->setSortRole(types::SortRole);
      proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
      proxyModel_->setFilterKeyColumn(-1);
   }
   ~AlertDockWidgetImpl() = default;

   void ConnectSignals();

   AlertDockWidget*                           self_;
   std::shared_ptr<manager::TextEventManager> textEventManager_;
   std::unique_ptr<model::AlertModel>         alertModel_;
   std::unique_ptr<model::AlertProxyModel>    proxyModel_;

   AlertDialog* alertDialog_;

   scwx::common::Coordinate mapPosition_;
   bool                     mapUpdateDeferred_;

   types::TextEventKey selectedAlertKey_;
   common::Coordinate  selectedAlertCentroid_;
};

AlertDockWidget::AlertDockWidget(QWidget* parent) :
    QDockWidget(parent),
    p {std::make_unique<AlertDockWidgetImpl>(this)},
    ui(new Ui::AlertDockWidget)
{
   ui->setupUi(this);

   ui->alertView->setModel(p->proxyModel_.get());
   ui->alertView->header()->setSortIndicator(
      static_cast<int>(model::AlertModel::Column::Distance),
      Qt::AscendingOrder);
   ui->alertView->header()->resizeSections(
      QHeaderView::ResizeMode::ResizeToContents);

   ui->alertSettings->addAction(ui->actionActiveAlerts);

   ui->alertViewButton->setEnabled(false);
   ui->alertGoButton->setEnabled(false);

   p->ConnectSignals();

   // Check Active Alerts and trigger signal
   ui->actionActiveAlerts->setChecked(true);
}

AlertDockWidget::~AlertDockWidget()
{
   delete ui;
}

void AlertDockWidget::showEvent(QShowEvent* event)
{
   if (p->mapUpdateDeferred_)
   {
      p->alertModel_->HandleMapUpdate(p->mapPosition_.latitude_,
                                      p->mapPosition_.longitude_);
      p->mapUpdateDeferred_ = false;
   }

   QDockWidget::showEvent(event);
}

void AlertDockWidget::HandleMapUpdate(double latitude, double longitude)
{
   p->mapPosition_ = {latitude, longitude};

   if (isVisible())
   {
      p->alertModel_->HandleMapUpdate(latitude, longitude);
   }
   else
   {
      p->mapUpdateDeferred_ = true;
   }
}

void AlertDockWidgetImpl::ConnectSignals()
{
   connect(self_->ui->alertFilter,
           &QLineEdit::textChanged,
           proxyModel_.get(),
           &QSortFilterProxyModel::setFilterWildcard);
   connect(self_->ui->actionActiveAlerts,
           &QAction::toggled,
           proxyModel_.get(),
           &model::AlertProxyModel::SetAlertActiveFilter);
   connect(textEventManager_.get(),
           &manager::TextEventManager::AlertUpdated,
           alertModel_.get(),
           &model::AlertModel::HandleAlert,
           Qt::QueuedConnection);
   connect(
      self_->ui->alertView->selectionModel(),
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

         bool itemSelected       = selected.size() > 0;
         bool itemHasCoordinates = false;

         if (itemSelected)
         {
            QModelIndex selectedIndex =
               proxyModel_->mapToSource(selected[0].indexes()[0]);
            selectedAlertKey_      = alertModel_->key(selectedIndex);
            selectedAlertCentroid_ = alertModel_->centroid(selectedAlertKey_);
            itemHasCoordinates =
               selectedAlertCentroid_ != common::Coordinate {};
         }
         else
         {
            selectedAlertKey_      = {};
            selectedAlertCentroid_ = {};
         }

         self_->ui->alertViewButton->setEnabled(itemSelected);
         self_->ui->alertGoButton->setEnabled(itemHasCoordinates);

         logger_->debug("Selected: {}", selectedAlertKey_.ToString());
      });
   connect(self_->ui->alertView,
           &QTreeView::doubleClicked,
           this,
           [this](const QModelIndex& /* index */)
           {
              // If an item is selected
              if (selectedAlertKey_ != types::TextEventKey {})
              {
                 types::AlertAction alertAction = types::GetAlertAction(
                    manager::SettingsManager::general_settings()
                       .default_alert_action()
                       .GetValue());

                 switch (alertAction)
                 {
                 case types::AlertAction::Go:
                    // Move map
                    Q_EMIT self_->MoveMap(selectedAlertCentroid_.latitude_,
                                          selectedAlertCentroid_.longitude_);
                    break;

                 case types::AlertAction::View:
                    // View alert
                    alertDialog_->SelectAlert(selectedAlertKey_);
                    alertDialog_->show();
                    break;

                 default:
                    // Do nothing
                    break;
                 }
              }
           });
   connect(self_->ui->alertViewButton,
           &QPushButton::clicked,
           this,
           [this]()
           {
              // View alert
              alertDialog_->SelectAlert(selectedAlertKey_);
              alertDialog_->show();
           });
   connect(self_->ui->alertGoButton,
           &QPushButton::clicked,
           this,
           [this]()
           {
              Q_EMIT self_->MoveMap(selectedAlertCentroid_.latitude_,
                                    selectedAlertCentroid_.longitude_);
           });
   connect(
      alertDialog_, &AlertDialog::MoveMap, self_, &AlertDockWidget::MoveMap);
}

#include "alert_dock_widget.moc"

} // namespace ui
} // namespace qt
} // namespace scwx
