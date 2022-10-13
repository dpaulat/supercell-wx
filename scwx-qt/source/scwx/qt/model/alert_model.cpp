#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/common/types.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <format>

#include <GeographicLib/Geodesic.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::alert_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t kColumnEtn          = 0u;
static constexpr size_t kColumnOfficeId     = 1u;
static constexpr size_t kColumnPhenomenon   = 2u;
static constexpr size_t kColumnSignificance = 3u;
static constexpr size_t kColumnState        = 4u;
static constexpr size_t kColumnCounties     = 5u;
static constexpr size_t kColumnStartTime    = 6u;
static constexpr size_t kColumnEndTime      = 7u;
static constexpr size_t kColumnDistance     = 8u;
static constexpr size_t kFirstColumn        = kColumnEtn;
static constexpr size_t kLastColumn         = kColumnDistance;
static constexpr size_t kNumColumns         = 9u;

class AlertModelImpl
{
public:
   explicit AlertModelImpl();
   ~AlertModelImpl() = default;

   QList<types::TextEventKey> textEventKeys_;

   GeographicLib::Geodesic geodesic_;

   std::unordered_map<types::TextEventKey,
                      double,
                      types::TextEventHash<types::TextEventKey>>
                              distanceMap_;
   scwx::common::DistanceType distanceDisplay_;
   scwx::common::Coordinate   previousPosition_;
};

AlertModel::AlertModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<AlertModelImpl>())
{
}
AlertModel::~AlertModel() = default;

int AlertModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : p->textEventKeys_.size();
}

int AlertModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : static_cast<int>(kNumColumns);
}

QVariant AlertModel::data(const QModelIndex& index, int role) const
{
   if (index.isValid() && index.row() >= 0 &&
       index.row() < p->textEventKeys_.size() &&
       (role == Qt::DisplayRole || role == common::SortRole))
   {
      const auto& textEventKey = p->textEventKeys_.at(index.row());

      switch (index.column())
      {
      case kColumnEtn:
         return textEventKey.etn_;
      case kColumnOfficeId:
         return QString::fromStdString(textEventKey.officeId_);
      case kColumnPhenomenon:
         return QString::fromStdString(
            awips::GetPhenomenonText(textEventKey.phenomenon_));
      case kColumnSignificance:
         return QString::fromStdString(
            awips::GetSignificanceText(textEventKey.significance_));
      case kColumnState:
         return QString::fromStdString("?");
      case kColumnCounties:
         return QString::fromStdString("?");
      case kColumnStartTime:
         return QString::fromStdString("?");
      case kColumnEndTime:
         return QString::fromStdString("?");
      case kColumnDistance:
         if (role == Qt::DisplayRole)
         {
            if (p->distanceDisplay_ == scwx::common::DistanceType::Miles)
            {
               return QString("%1 mi").arg(
                  static_cast<uint32_t>(p->distanceMap_.at(textEventKey) *
                                        scwx::common::kMilesPerMeter));
            }
            else
            {
               return QString("%1 km").arg(
                  static_cast<uint32_t>(p->distanceMap_.at(textEventKey) *
                                        scwx::common::kKilometersPerMeter));
            }
         }
         else
         {
            return p->distanceMap_.at(textEventKey);
         }
      default:
         break;
      }
   }

   return QVariant();
}

QVariant
AlertModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (role == Qt::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
         case kColumnEtn:
            return tr("ETN");
         case kColumnOfficeId:
            return tr("Office ID");
         case kColumnPhenomenon:
            return tr("Phenomenon");
         case kColumnSignificance:
            return tr("Significance");
         case kColumnState:
            return tr("State");
         case kColumnCounties:
            return tr("Counties");
         case kColumnStartTime:
            return tr("Start Time");
         case kColumnEndTime:
            return tr("End Time");
         case kColumnDistance:
            return tr("Distance");
         default:
            break;
         }
      }
   }

   return QVariant();
}

void AlertModel::HandleAlert(const types::TextEventKey& alertKey)
{
   logger_->trace("Handle alert: {}, {}, {}, {}",
                  alertKey.etn_,
                  alertKey.officeId_,
                  awips::GetPhenomenonText(alertKey.phenomenon_),
                  awips::GetSignificanceText(alertKey.significance_));

   double distanceInMeters;

   if (!p->textEventKeys_.contains(alertKey))
   {
      beginInsertRows(QModelIndex(), 0, 0);

      p->textEventKeys_.push_back(alertKey);

      p->geodesic_.Inverse(p->previousPosition_.latitude_,
                           p->previousPosition_.longitude_,
                           0.0, // TODO: textEvent->latitude(),
                           0.0, // TODO: textEvent->longitude(),
                           distanceInMeters);

      p->distanceMap_[alertKey] = distanceInMeters;

      endInsertRows();
   }
   else
   {
      p->geodesic_.Inverse(p->previousPosition_.latitude_,
                           p->previousPosition_.longitude_,
                           0.0, // TODO: textEvent->latitude(),
                           0.0, // TODO: textEvent->longitude(),
                           distanceInMeters);

      const int   row         = 0; // TODO
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      emit dataChanged(topLeft, bottomRight);
   }
}

void AlertModel::HandleMapUpdate(double latitude, double longitude)
{
   logger_->trace("Handle map update: {}, {}", latitude, longitude);

   double distanceInMeters;

   for (const auto& textEvent : p->textEventKeys_)
   {
      p->geodesic_.Inverse(latitude,
                           longitude,
                           0.0, // TODO: textEvent->latitude(),
                           0.0, // TODO: textEvent->longitude(),
                           distanceInMeters);
      p->distanceMap_[textEvent] = distanceInMeters;
   }

   p->previousPosition_ = {latitude, longitude};

   QModelIndex topLeft     = createIndex(0, kColumnDistance);
   QModelIndex bottomRight = createIndex(rowCount() - 1, kColumnDistance);

   emit dataChanged(topLeft, bottomRight);
}

AlertModelImpl::AlertModelImpl() :
    textEventKeys_ {},
    geodesic_(GeographicLib::Constants::WGS84_a(),
              GeographicLib::Constants::WGS84_f()),
    distanceMap_ {},
    distanceDisplay_ {scwx::common::DistanceType::Miles},
    previousPosition_ {}
{
}

} // namespace model
} // namespace qt
} // namespace scwx
