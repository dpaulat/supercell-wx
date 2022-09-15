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

   ~RadarProductModelImpl() {}
};

RadarProductModel::RadarProductModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<RadarProductModelImpl>())
{
}
RadarProductModel::~RadarProductModel() = default;

} // namespace model
} // namespace qt
} // namespace scwx
