#include <scwx/qt/model/radar_product_model.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::radar_product_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RadarProductModelImpl
{
public:
   explicit RadarProductModelImpl() :
       rootItem_ {std::make_shared<TreeItem>(
          std::vector<QVariant> {QObject::tr("Name"), QObject::tr("Info")})}
   {
      rootItem_->AppendChild(new TreeItem(
         std::vector<QVariant> {QObject::tr("MyItem"), QObject::tr("Data")}));
   }
   ~RadarProductModelImpl() = default;

   std::shared_ptr<TreeItem> rootItem_;
};

RadarProductModel::RadarProductModel(QObject* parent) :
    TreeModel(parent), p(std::make_unique<RadarProductModelImpl>())
{
}
RadarProductModel::~RadarProductModel() = default;

const std::shared_ptr<TreeItem> RadarProductModel::root_item() const
{
   return p->rootItem_;
}

} // namespace model
} // namespace qt
} // namespace scwx
