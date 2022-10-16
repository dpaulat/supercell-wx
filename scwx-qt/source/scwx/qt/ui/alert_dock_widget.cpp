#include "alert_dock_widget.hpp"
#include "ui_alert_dock_widget.h"

#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <QSortFilterProxyModel>

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
       alertModel_ {std::make_unique<model::AlertModel>()},
       proxyModel_ {std::make_unique<QSortFilterProxyModel>()},
       mapPosition_ {},
       mapUpdateDeferred_ {false}
   {
      proxyModel_->setSourceModel(alertModel_.get());
      proxyModel_->setSortRole(types::SortRole);
      proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
      proxyModel_->setFilterKeyColumn(-1);
   }
   ~AlertDockWidgetImpl() = default;

   void ConnectSignals();

   AlertDockWidget*                       self_;
   std::unique_ptr<model::AlertModel>     alertModel_;
   std::unique_ptr<QSortFilterProxyModel> proxyModel_;

   scwx::common::Coordinate mapPosition_;
   bool                     mapUpdateDeferred_;
};

AlertDockWidget::AlertDockWidget(QWidget* parent) :
    QDockWidget(parent),
    p {std::make_unique<AlertDockWidgetImpl>(this)},
    ui(new Ui::AlertDockWidget)
{
   ui->setupUi(this);

   ui->alertView->setModel(p->proxyModel_.get());

   ui->alertSettings->addAction(ui->actionActiveAlerts);

   p->ConnectSignals();
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
   connect(&manager::TextEventManager::Instance(),
           &manager::TextEventManager::AlertUpdated,
           alertModel_.get(),
           &model::AlertModel::HandleAlert,
           Qt::QueuedConnection);
   connect(self_->ui->alertView->selectionModel(),
           &QItemSelectionModel::selectionChanged,
           this,
           [=](const QItemSelection& selected, const QItemSelection& deselected)
           {
              if (selected.size() == 0 && deselected.size() == 0)
              {
                 // Items which stay selected but change their index are not
                 // included in selected and deselected. Thus, this signal might
                 // be emitted with both selected and deselected empty, if only
                 // the indices of selected items change.
                 return;
              }
           });
   connect(self_->ui->alertViewButton,
           &QPushButton::clicked,
           this,
           []()
           {
              // TODO: View alert
           });
   connect(self_->ui->alertGoButton,
           &QPushButton::clicked,
           this,
           []()
           {
              // TODO: Go to alert
           });
}

#include "alert_dock_widget.moc"

} // namespace ui
} // namespace qt
} // namespace scwx
