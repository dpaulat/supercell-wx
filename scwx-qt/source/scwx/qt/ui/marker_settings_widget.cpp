#include "marker_settings_widget.hpp"
#include "ui_marker_settings_widget.h"

#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/qt/model/marker_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/ui/open_url_dialog.hpp>
#include <scwx/util/logger.hpp>

#include <QSortFilterProxyModel>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::marker_settings_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MarkerSettingsWidgetImpl
{
public:
   explicit MarkerSettingsWidgetImpl(MarkerSettingsWidget* self) :
      self_ {self},
      markerModel_ {new model::MarkerModel(self_)}
   {
   }

   void ConnectSignals();

   MarkerSettingsWidget* self_;
   model::MarkerModel* markerModel_;
   std::shared_ptr<manager::MarkerManager> markerManager_ {
      manager::MarkerManager::Instance()};
};


MarkerSettingsWidget::MarkerSettingsWidget(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<MarkerSettingsWidgetImpl>(this)},
    ui(new Ui::MarkerSettingsWidget)
{
   ui->setupUi(this);

   ui->removeButton->setEnabled(false);

   ui->markerView->setModel(p->markerModel_);

   p->ConnectSignals();
}

MarkerSettingsWidget::~MarkerSettingsWidget()
{
   delete ui;
}

void MarkerSettingsWidgetImpl::ConnectSignals()
{
   QObject::connect(self_->ui->addButton,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       markerManager_->add_marker(types::MarkerInfo("", 0, 0));
                    });
   QObject::connect(self_->ui->removeButton,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       auto selectionModel =
                          self_->ui->markerView->selectionModel();
                       QModelIndex selected =
                          selectionModel
                             ->selectedRows(static_cast<int>(
                                model::MarkerModel::Column::Name))
                             .first();

                       markerManager_->remove_marker(selected.row());
                    });
   QObject::connect(
      self_->ui->markerView->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      self_,
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

         bool itemSelected = selected.size() > 0;
         self_->ui->removeButton->setEnabled(itemSelected);
      });
}

} // namespace ui
} // namespace qt
} // namespace scwx
