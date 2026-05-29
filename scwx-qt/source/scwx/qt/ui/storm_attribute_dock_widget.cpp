#include <scwx/qt/ui/storm_attribute_dock_widget.hpp>
#include <scwx/qt/model/storm_attribute_model.hpp>
#include <scwx/qt/model/storm_attribute_proxy_model.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/wsr88d/rpg/storm_tracking_information_message.hpp>

#include <QHeaderView>
#include <QLineEdit>
#include <QShowEvent>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace scwx::qt::ui
{

class StormAttributeDockWidgetImpl
{
public:
   explicit StormAttributeDockWidgetImpl(StormAttributeDockWidget* self) :
       self_(self)
   {
   }
   ~StormAttributeDockWidgetImpl() = default;

   void SetupUi();
   void ConnectSignals();

   StormAttributeDockWidget* self_;

   std::shared_ptr<manager::RadarProductManager> radarProductManager_ {};
   std::shared_ptr<view::OverlayProductView>     overlayProductView_ {};

   model::StormAttributeModel*      stormAttributeModel_ {nullptr};
   model::StormAttributeProxyModel* proxyModel_ {nullptr};

   QTreeView* stormAttributeView_ {nullptr};
   QLineEdit* stormAttributeFilter_ {nullptr};
   QSpinBox*  minDbzFilter_ {nullptr};

   common::Coordinate mapPosition_ {};
   bool               mapUpdateDeferred_ {false};
};

StormAttributeDockWidget::StormAttributeDockWidget(QWidget* parent) :
    QDockWidget(parent), p(std::make_unique<StormAttributeDockWidgetImpl>(this))
{
   p->SetupUi();
   p->ConnectSignals();
}

StormAttributeDockWidget::~StormAttributeDockWidget() = default;

void StormAttributeDockWidgetImpl::SetupUi()
{
   self_->setObjectName("StormAttributeDockWidget");
   self_->setWindowTitle(self_->tr("Storm Attributes"));
   self_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
   self_->setFeatures(QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable |
                      QDockWidget::DockWidgetClosable);
   self_->setMinimumWidth(500);

   auto* widget = new QWidget(self_);
   auto* layout = new QVBoxLayout(widget);
   layout->setContentsMargins(0, 0, 0, 0);

   // Filter bar
   auto* filterLayout = new QHBoxLayout();
   filterLayout->setContentsMargins(4, 4, 4, 4);

   auto* filterLabel = new QLabel(self_->tr("Filter:"), widget);
   filterLayout->addWidget(filterLabel);

   stormAttributeFilter_ = new QLineEdit(widget);
   stormAttributeFilter_->setPlaceholderText(
      self_->tr("Filter by storm ID..."));
   stormAttributeFilter_->setClearButtonEnabled(true);
   filterLayout->addWidget(stormAttributeFilter_);

   auto* minDbzLabel = new QLabel(self_->tr("Min dBZ:"), widget);
   filterLayout->addWidget(minDbzLabel);

   minDbzFilter_ = new QSpinBox(widget);
   minDbzFilter_->setRange(0, 99);
   minDbzFilter_->setValue(0);
   minDbzFilter_->setSuffix(" dBZ");
   minDbzFilter_->setSpecialValueText(self_->tr("Off"));
   filterLayout->addWidget(minDbzFilter_);

   layout->addLayout(filterLayout);

   // Tree view (flat table)
   stormAttributeView_ = new QTreeView(widget);
   stormAttributeView_->setAlternatingRowColors(true);
   stormAttributeView_->setIndentation(0);
   stormAttributeView_->setSortingEnabled(true);
   stormAttributeView_->setSelectionBehavior(QAbstractItemView::SelectRows);
   stormAttributeView_->setSelectionMode(QAbstractItemView::SingleSelection);
   stormAttributeView_->setRootIsDecorated(false);
   layout->addWidget(stormAttributeView_);

   self_->setWidget(widget);

   // Create models
   auto* model          = new model::StormAttributeModel(self_);
   stormAttributeModel_ = model;

   auto* proxy = new model::StormAttributeProxyModel(self_);
   proxy->setSourceModel(model);
   proxyModel_ = proxy;

   stormAttributeView_->setModel(proxy);

   // Set default sort by Max dBZ descending
   stormAttributeView_->header()->setSortIndicator(
      static_cast<int>(model::StormAttributeModel::Column::MaxDbz),
      Qt::DescendingOrder);
   stormAttributeView_->header()->resizeSections(QHeaderView::ResizeToContents);
}

void StormAttributeDockWidgetImpl::ConnectSignals()
{
   // Filter text -> proxy model
   QObject::connect(stormAttributeFilter_,
                    &QLineEdit::textChanged,
                    proxyModel_,
                    &QSortFilterProxyModel::setFilterFixedString);

   // Min dBZ -> proxy model
   QObject::connect(minDbzFilter_,
                    QOverload<int>::of(&QSpinBox::valueChanged),
                    proxyModel_,
                    &model::StormAttributeProxyModel::SetMinDbzFilter);

   // Double-click -> zoom to storm
   QObject::connect(
      stormAttributeView_,
      &QTreeView::doubleClicked,
      self_,
      [this](const QModelIndex& index)
      {
         QModelIndex sourceIndex = proxyModel_->mapToSource(index);
         const auto& attr =
            stormAttributeModel_->attributeAt(sourceIndex.row());

         if (radarProductManager_ != nullptr && attr.azimuth.has_value() &&
             attr.range.has_value())
         {
            auto radarSite = radarProductManager_->radar_site();
            if (radarSite != nullptr)
            {
               auto coord = scwx::common::polar_to_latlon(
                  radarSite->latitude(),
                  radarSite->longitude(),
                  radarSite->altitude().value() * 0.3048, // feet -> meters
                  static_cast<double>(attr.azimuth.value()),
                  static_cast<double>(attr.range.value()));
               Q_EMIT self_->ZoomToStorm(coord.latitude_, coord.longitude_);
            }
         }
      });
}

void StormAttributeDockWidget::SetRadarProductManager(
   const std::shared_ptr<manager::RadarProductManager>& radarProductManager)
{
   p->radarProductManager_ = radarProductManager;
}

void StormAttributeDockWidget::SetOverlayProductView(
   const std::shared_ptr<view::OverlayProductView>& overlayProductView)
{
   p->overlayProductView_ = overlayProductView;
}

void StormAttributeDockWidget::HandleDataUpdate()
{
   if (p->overlayProductView_ == nullptr || p->radarProductManager_ == nullptr)
   {
      return;
   }

   auto message = p->overlayProductView_->radar_product_message("NST");
   if (message == nullptr)
   {
      p->stormAttributeModel_->ClearData();
      return;
   }

   auto sti =
      std::dynamic_pointer_cast<wsr88d::rpg::StormTrackingInformationMessage>(
         message);
   if (sti == nullptr)
   {
      p->stormAttributeModel_->ClearData();
      return;
   }

   // Get radar site position for distance calculation
   common::Coordinate radarSitePos {0.0, 0.0};
   auto               radarSite = p->radarProductManager_->radar_site();
   if (radarSite != nullptr)
   {
      radarSitePos = {radarSite->latitude(), radarSite->longitude()};
   }

   std::vector<model::StormAttributeModel::StormAttribute> attributes;

   for (const auto& [id, record] : sti->sti_records())
   {
      if (record == nullptr)
      {
         continue;
      }

      model::StormAttributeModel::StormAttribute attr;
      attr.stormId = id;

      if (record->currentPosition_.azimuth_.has_value())
      {
         attr.azimuth = record->currentPosition_.azimuth_->value();
      }
      if (record->currentPosition_.range_.has_value())
      {
         attr.range = record->currentPosition_.range_->value();
      }
      if (record->direction_.has_value())
      {
         attr.direction = record->direction_->value();
      }
      if (record->speed_.has_value())
      {
         attr.speed = record->speed_->value();
      }
      if (record->maxDbz_.has_value())
      {
         attr.maxDbz = record->maxDbz_.value();
      }
      if (record->maxDbzHeight_.has_value())
      {
         attr.maxDbzHeight = record->maxDbzHeight_->value();
      }
      if (record->forecastError_.has_value())
      {
         attr.forecastError = record->forecastError_->value();
      }

      attributes.push_back(std::move(attr));
   }

   p->stormAttributeModel_->UpdateData(attributes, radarSitePos);
}

void StormAttributeDockWidget::HandleMapUpdate(double latitude,
                                               double longitude)
{
   p->mapPosition_ = {latitude, longitude};

   if (isVisible())
   {
      p->stormAttributeModel_->HandleMapUpdate(latitude, longitude);
   }
   else
   {
      p->mapUpdateDeferred_ = true;
   }
}

void StormAttributeDockWidget::showEvent(QShowEvent* event)
{
   if (p->mapUpdateDeferred_)
   {
      p->stormAttributeModel_->HandleMapUpdate(p->mapPosition_.latitude_,
                                               p->mapPosition_.longitude_);
      p->mapUpdateDeferred_ = false;
   }

   QDockWidget::showEvent(event);
}

} // namespace scwx::qt::ui
