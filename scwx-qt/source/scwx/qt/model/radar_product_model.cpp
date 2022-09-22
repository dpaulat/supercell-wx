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
       std::vector<QVariant> {QObject::tr("Name"), QObject::tr("Info")})}
{
   connect(
      &manager::RadarProductManagerNotifier::Instance(),
      &manager::RadarProductManagerNotifier::RadarProductManagerCreated,
      this,
      [=](const std::string& radarSite)
      {
         TreeItem* radarSiteItem =
            new TreeItem({QString::fromStdString(radarSite)});

         rootItem_->AppendChild(radarSiteItem);

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
                  groupItem = new TreeItem({groupName});
                  radarSiteItem->AppendChild(groupItem);
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
                     productItem = new TreeItem({productName});
                     groupItem->AppendChild(productItem);
                  }
               }

               // Create leaf item for product time
               productItem->AppendChild(new TreeItem {
                  QString::fromStdString(util::TimeString(latestTime))});
            },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}

#include "radar_product_model.moc"

} // namespace model
} // namespace qt
} // namespace scwx
