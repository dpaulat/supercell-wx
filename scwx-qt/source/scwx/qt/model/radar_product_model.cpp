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
   explicit RadarProductModelImpl() {}
   ~RadarProductModelImpl() = default;
};

RadarProductModel::RadarProductModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<RadarProductModelImpl>())
{
}
RadarProductModel::~RadarProductModel() = default;

int RadarProductModel::rowCount(const QModelIndex& /*parent*/) const
{
   return 0;
}

int RadarProductModel::columnCount(const QModelIndex& /*parent*/) const
{
   return 0;
}

QVariant RadarProductModel::data(const QModelIndex& /*index*/,
                                 int /*role*/) const
{
   return QVariant();
}

} // namespace model
} // namespace qt
} // namespace scwx
