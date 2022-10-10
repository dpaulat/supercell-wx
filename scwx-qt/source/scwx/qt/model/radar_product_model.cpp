#include <scwx/qt/model/radar_product_model.hpp>
#include <scwx/qt/model/tree_model.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/radar_product_manager_notifier.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::radar_product_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RadarProductModelImpl : public QObject
{
   Q_OBJECT

public:
   explicit RadarProductModelImpl(RadarProductModel* self);
   ~RadarProductModelImpl() = default;

   RadarProductModel*         self_;
   std::unique_ptr<TreeModel> model_;
};

RadarProductModel::RadarProductModel() :
    p(std::make_unique<RadarProductModelImpl>(this))
{
}
RadarProductModel::~RadarProductModel() = default;

QAbstractItemModel* RadarProductModel::model()
{
   return p->model_.get();
}

RadarProductModelImpl::RadarProductModelImpl(RadarProductModel* self) :
    self_ {self},
    model_ {std::make_unique<TreeModel>(
       std::vector<QVariant> {QObject::tr("Product")})}
{
   connect(
      &manager::RadarProductManagerNotifier::Instance(),
      &manager::RadarProductManagerNotifier::RadarProductManagerCreated,
      this,
      [=](const std::string& radarSite)
      {
         logger_->debug("Adding radar site: {}", radarSite);

         const QString radarSiteName {QString::fromStdString(radarSite)};

         // Find existing radar site item (e.g., KLSX, KEAX)
         TreeItem* radarSiteItem =
            model_->root_item()->FindChild(0, radarSiteName);

         if (radarSiteItem == nullptr)
         {
            radarSiteItem = new TreeItem({radarSiteName});
            model_->AppendRow(model_->root_item(), radarSiteItem);
         }

         connect(
            manager::RadarProductManager::Instance(radarSite).get(),
            &manager::RadarProductManager::NewDataAvailable,
            this,
            [=](common::RadarProductGroup             group,
                const std::string&                    product,
                std::chrono::system_clock::time_point latestTime)
            {
               const QString groupName {QString::fromStdString(
                  common::GetRadarProductGroupName(group))};

               // Find existing group item (e.g., Level 2, Level 3)
               TreeItem* groupItem = radarSiteItem->FindChild(0, groupName);

               if (groupItem == nullptr)
               {
                  // Existing group item was not found, create it
                  groupItem = new TreeItem({groupName});
                  model_->AppendRow(radarSiteItem, groupItem);
               }

               TreeItem* productItem = nullptr;

               if (group == common::RadarProductGroup::Level2)
               {
                  // Level 2 items are not separated by product
                  productItem = groupItem;
               }
               else
               {
                  // Find existing product item (e.g., N0B, N0Q)
                  const QString productName {QString::fromStdString(product)};
                  productItem = groupItem->FindChild(0, productName);

                  if (productItem == nullptr)
                  {
                     // Existing product item was not found, create it
                     productItem = new TreeItem({productName});
                     model_->AppendRow(groupItem, productItem);
                  }
               }

               // Create leaf item for product time
               model_->AppendRow(productItem,
                                 new TreeItem {QString::fromStdString(
                                    util::TimeString(latestTime))});
            },
            Qt::QueuedConnection);
      });
}

#include "radar_product_model.moc"

} // namespace model
} // namespace qt
} // namespace scwx
