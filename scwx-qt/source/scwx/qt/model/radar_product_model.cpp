#include <scwx/qt/model/radar_product_model.hpp>
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

   void AppendRow(TreeItem* parent, TreeItem* child);

   RadarProductModel*        self_;
   std::shared_ptr<TreeItem> rootItem_;
};

RadarProductModel::RadarProductModel(QObject* parent) :
    TreeModel(parent), p(std::make_unique<RadarProductModelImpl>(this))
{
}
RadarProductModel::~RadarProductModel() = default;

const std::shared_ptr<TreeItem> RadarProductModel::root_item() const
{
   return p->rootItem_;
}

void RadarProductModelImpl::AppendRow(TreeItem* parent, TreeItem* child)
{
   const QModelIndex parentIndex = self_->createIndex(parent->row(), 0, parent);
   const int         childCount  = parent->child_count();
   const int         first       = childCount;
   const int         last        = childCount;

   self_->beginInsertRows(parentIndex, first, last);
   parent->AppendChild(child);
   self_->endInsertRows();
}

RadarProductModelImpl::RadarProductModelImpl(RadarProductModel* self) :
    self_ {self},
    rootItem_ {std::make_shared<TreeItem>(
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
         TreeItem* radarSiteItem = rootItem_->FindChild(0, radarSiteName);

         if (radarSiteItem == nullptr)
         {
            radarSiteItem = new TreeItem({radarSiteName});
            AppendRow(rootItem_.get(), radarSiteItem);
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
                  AppendRow(radarSiteItem, groupItem);
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
                     AppendRow(groupItem, productItem);
                  }
               }

               // Create leaf item for product time
               AppendRow(productItem,
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
