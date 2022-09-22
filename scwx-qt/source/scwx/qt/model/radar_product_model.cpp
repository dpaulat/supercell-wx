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

         const QModelIndex rootIndex =
            self_->createIndex(rootItem_->row(), 0, rootItem_.get());
         const int rootChildren = rootItem_->child_count();

         self_->beginInsertRows(rootIndex, rootChildren, rootChildren);

         TreeItem* radarSiteItem =
            new TreeItem({QString::fromStdString(radarSite)});
         rootItem_->AppendChild(radarSiteItem);

         self_->endInsertRows();

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
                  const QModelIndex radarSiteIndex =
                     self_->createIndex(radarSiteItem->row(), 0, radarSiteItem);
                  const int radarSiteChildren = radarSiteItem->child_count();

                  self_->beginInsertRows(
                     radarSiteIndex, radarSiteChildren, radarSiteChildren);

                  groupItem = new TreeItem({groupName});
                  radarSiteItem->AppendChild(groupItem);

                  self_->endInsertRows();
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
                     const QModelIndex groupItemIndex =
                        self_->createIndex(groupItem->row(), 0, groupItem);
                     const int groupItemChildren = groupItem->child_count();

                     self_->beginInsertRows(
                        groupItemIndex, groupItemChildren, groupItemChildren);

                     productItem = new TreeItem({productName});
                     groupItem->AppendChild(productItem);

                     self_->endInsertRows();
                  }
               }

               // Create leaf item for product time
               const QModelIndex productItemIndex =
                  self_->createIndex(productItem->row(), 0, productItem);
               const int productItemChildren = productItem->child_count();

               self_->beginInsertRows(
                  productItemIndex, productItemChildren, productItemChildren);

               productItem->AppendChild(new TreeItem {
                  QString::fromStdString(util::TimeString(latestTime))});

               self_->endInsertRows();
            },
            Qt::QueuedConnection);
      });
}

#include "radar_product_model.moc"

} // namespace model
} // namespace qt
} // namespace scwx
