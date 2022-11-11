#include <scwx/qt/model/alert_proxy_model.hpp>
#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::alert_proxy_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertProxyModelImpl
{
public:
   explicit AlertProxyModelImpl();
   ~AlertProxyModelImpl() = default;

   bool alertActiveFilterEnabled_;
};

AlertProxyModel::AlertProxyModel(QObject* parent) :
    QSortFilterProxyModel(parent), p(std::make_unique<AlertProxyModelImpl>())
{
}
AlertProxyModel::~AlertProxyModel() = default;

void AlertProxyModel::SetAlertActiveFilter(bool enabled)
{
   p->alertActiveFilterEnabled_ = enabled;
   invalidateRowsFilter();
}

bool AlertProxyModel::filterAcceptsRow(int                sourceRow,
                                       const QModelIndex& sourceParent) const
{
   bool acceptAlertActiveFilter = true;

   if (p->alertActiveFilterEnabled_)
   {
      // Get source model index
      QModelIndex endTimeIndex =
         sourceModel()->index(sourceRow,
                              static_cast<int>(AlertModel::Column::EndTime),
                              sourceParent);

      // Get source end time
      auto endTime = sourceModel()
                        ->data(endTimeIndex, qt::types::SortRole)
                        .value<std::chrono::system_clock::time_point>();

      // Compare end time to current
      if (endTime < std::chrono::system_clock::now())
      {
         acceptAlertActiveFilter = false;
      }
   }

   return acceptAlertActiveFilter &&
          QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

AlertProxyModelImpl::AlertProxyModelImpl() : alertActiveFilterEnabled_ {false}
{
}

} // namespace model
} // namespace qt
} // namespace scwx
