#include "placefile_settings_widget.hpp"
#include "ui_placefile_settings_widget.h"

#include <scwx/qt/model/placefile_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/ui/left_elided_item_delegate.hpp>
#include <scwx/qt/ui/open_url_dialog.hpp>
#include <scwx/util/logger.hpp>

#include <QSortFilterProxyModel>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::placefile_settings_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileSettingsWidgetImpl
{
public:
   explicit PlacefileSettingsWidgetImpl(PlacefileSettingsWidget* self) :
       self_ {self},
       openUrlDialog_ {new OpenUrlDialog(QObject::tr("Add Placefile"), self_)},
       placefileModel_ {new model::PlacefileModel(self_)},
       placefileProxyModel_ {new QSortFilterProxyModel(self_)},
       leftElidedItemDelegate_ {new LeftElidedItemDelegate(self_)}
   {
      placefileProxyModel_->setSourceModel(placefileModel_);
      placefileProxyModel_->setSortRole(types::SortRole);
      placefileProxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
      placefileProxyModel_->setFilterKeyColumn(-1);
   }
   ~PlacefileSettingsWidgetImpl() = default;

   void ConnectSignals();

   PlacefileSettingsWidget* self_;
   OpenUrlDialog*           openUrlDialog_;

   model::PlacefileModel*  placefileModel_;
   QSortFilterProxyModel*  placefileProxyModel_;
   LeftElidedItemDelegate* leftElidedItemDelegate_;
};

PlacefileSettingsWidget::PlacefileSettingsWidget(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<PlacefileSettingsWidgetImpl>(this)},
    ui(new Ui::PlacefileSettingsWidget)
{
   ui->setupUi(this);

   ui->placefileView->setModel(p->placefileProxyModel_);
   ui->placefileView->header()->setSortIndicator(
      static_cast<int>(model::PlacefileModel::Column::Url), Qt::AscendingOrder);

   ui->placefileView->resizeColumnToContents(
      static_cast<int>(model::PlacefileModel::Column::Enabled));
   ui->placefileView->resizeColumnToContents(
      static_cast<int>(model::PlacefileModel::Column::Thresholds));
   ui->placefileView->resizeColumnToContents(
      static_cast<int>(model::PlacefileModel::Column::Url));

   ui->placefileView->setItemDelegateForColumn(
      static_cast<int>(model::PlacefileModel::Column::Url),
      p->leftElidedItemDelegate_);

   p->ConnectSignals();
}

PlacefileSettingsWidget::~PlacefileSettingsWidget()
{
   delete ui;
}

void PlacefileSettingsWidgetImpl::ConnectSignals()
{
   QObject::connect(self_->ui->addButton,
                    &QPushButton::clicked,
                    self_,
                    [this]() { openUrlDialog_->open(); });

   QObject::connect(
      openUrlDialog_,
      &OpenUrlDialog::accepted,
      self_,
      [this]()
      { logger_->info("Add URL: {}", openUrlDialog_->url().toStdString()); });

   QObject::connect(self_->ui->placefileFilter,
                    &QLineEdit::textChanged,
                    placefileProxyModel_,
                    &QSortFilterProxyModel::setFilterWildcard);
}

} // namespace ui
} // namespace qt
} // namespace scwx
