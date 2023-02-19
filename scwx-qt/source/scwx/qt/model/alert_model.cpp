#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/strings.hpp>
#include <scwx/util/time.hpp>

#include <format>

#include <QApplication>
#include <QFontMetrics>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::alert_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn = static_cast<int>(AlertModel::Column::Etn);
static constexpr int kLastColumn =
   static_cast<int>(AlertModel::Column::Distance);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

class AlertModelImpl
{
public:
   explicit AlertModelImpl();
   ~AlertModelImpl() = default;

   static std::string GetCounties(const types::TextEventKey& key);
   static std::string GetState(const types::TextEventKey& key);
   static std::chrono::system_clock::time_point
                      GetStartTime(const types::TextEventKey& key);
   static std::string GetStartTimeString(const types::TextEventKey& key);
   static std::chrono::system_clock::time_point
                      GetEndTime(const types::TextEventKey& key);
   static std::string GetEndTimeString(const types::TextEventKey& key);

   std::shared_ptr<manager::TextEventManager> textEventManager_;

   QList<types::TextEventKey> textEventKeys_;

   const GeographicLib::Geodesic& geodesic_;

   std::unordered_map<types::TextEventKey,
                      common::Coordinate,
                      types::TextEventHash<types::TextEventKey>>
      centroidMap_;
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

types::TextEventKey AlertModel::key(const QModelIndex& index) const
{
   return index.isValid() ? p->textEventKeys_[index.row()] :
                            types::TextEventKey {};
}

common::Coordinate AlertModel::centroid(const types::TextEventKey& key) const
{
   common::Coordinate centroid {};

   const auto& it = p->centroidMap_.find(key);
   if (it != p->centroidMap_.cend())
   {
      centroid = it->second;
   }

   return centroid;
}

int AlertModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : p->textEventKeys_.size();
}

int AlertModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : kNumColumns;
}

QVariant AlertModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 ||
       index.row() >= p->textEventKeys_.size())
   {
      return QVariant();
   }

   const auto& textEventKey = p->textEventKeys_.at(index.row());

   if (role == Qt::ItemDataRole::DisplayRole ||
       role == types::ItemDataRole::SortRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::Etn):
         return textEventKey.etn_;

      case static_cast<int>(Column::OfficeId):
         return QString::fromStdString(textEventKey.officeId_);

      case static_cast<int>(Column::Phenomenon):
         return QString::fromStdString(
            awips::GetPhenomenonText(textEventKey.phenomenon_));

      case static_cast<int>(Column::Significance):
         return QString::fromStdString(
            awips::GetSignificanceText(textEventKey.significance_));

      case static_cast<int>(Column::State):
         return QString::fromStdString(AlertModelImpl::GetState(textEventKey));

      case static_cast<int>(Column::Counties):
         return QString::fromStdString(
            AlertModelImpl::GetCounties(textEventKey));

      case static_cast<int>(Column::StartTime):
         return QString::fromStdString(
            AlertModelImpl::GetStartTimeString(textEventKey));

      case static_cast<int>(Column::EndTime):
         return QString::fromStdString(
            AlertModelImpl::GetEndTimeString(textEventKey));

      case static_cast<int>(Column::Distance):
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
   else if (role == types::ItemDataRole::TimePointRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::StartTime):
         return QVariant::fromValue(AlertModelImpl::GetStartTime(textEventKey));

      case static_cast<int>(Column::EndTime):
         return QVariant::fromValue(AlertModelImpl::GetEndTime(textEventKey));

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
         case static_cast<int>(Column::Etn):
            return tr("ETN");

         case static_cast<int>(Column::OfficeId):
            return tr("Office ID");

         case static_cast<int>(Column::Phenomenon):
            return tr("Phenomenon");

         case static_cast<int>(Column::Significance):
            return tr("Significance");

         case static_cast<int>(Column::State):
            return tr("State");

         case static_cast<int>(Column::Counties):
            return tr("Counties / Areas");

         case static_cast<int>(Column::StartTime):
            return tr("Start Time");

         case static_cast<int>(Column::EndTime):
            return tr("End Time");

         case static_cast<int>(Column::Distance):
            return tr("Distance");

         default:
            break;
         }
      }
   }
   else if (role == Qt::ItemDataRole::SizeHintRole)
   {
      static const QFontMetrics fontMetrics(QApplication::font());

      QSize contentsSize {};

      switch (section)
      {
      case static_cast<int>(Column::Etn):
         contentsSize = fontMetrics.size(0, "0000"); // 24x16
         break;

      case static_cast<int>(Column::Phenomenon):
         contentsSize = fontMetrics.size(0, QString(10, 'W'));
         break;

      case static_cast<int>(Column::State):
         contentsSize = fontMetrics.size(0, "WW, WW");
         break;

      case static_cast<int>(Column::Counties):
         contentsSize = fontMetrics.size(0, QString(15, 'W'));
         break;

      case static_cast<int>(Column::StartTime):
      case static_cast<int>(Column::EndTime):
         contentsSize = fontMetrics.size(0, "0000-00-00 00:00:00");
         break;

      case static_cast<int>(Column::Distance):
         contentsSize = fontMetrics.size(0, "00000 km");
         break;

      default:
         break;
      }

      if (contentsSize != QSize {})
      {
         // Add padding
         // TODO: Ideally, derived using the current style, but how?
         contentsSize += QSize(12, 8);
         return QVariant(contentsSize);
      }
   }

   return QVariant();
}

void AlertModel::HandleAlert(const types::TextEventKey& alertKey,
                             size_t                     messageIndex)
{
   logger_->trace("Handle alert: {}", alertKey.ToString());

   double distanceInMeters;

   // Get the most recent segment for the event
   auto alertMessages = p->textEventManager_->message_list(alertKey);
   std::shared_ptr<const awips::Segment> alertSegment =
      alertMessages[messageIndex]->segments().back();

   if (alertSegment->codedLocation_.has_value())
   {
      // Update centroid and distance
      common::Coordinate centroid =
         common::GetCentroid(alertSegment->codedLocation_->coordinates());

      p->geodesic_.Inverse(p->previousPosition_.latitude_,
                           p->previousPosition_.longitude_,
                           centroid.latitude_,
                           centroid.longitude_,
                           distanceInMeters);

      p->centroidMap_.insert_or_assign(alertKey, centroid);
      p->distanceMap_.insert_or_assign(alertKey, distanceInMeters);
   }
   else if (!p->centroidMap_.contains(alertKey))
   {
      // The alert has no location, so provide a default
      p->centroidMap_.insert_or_assign(alertKey, common::Coordinate {0.0, 0.0});
      p->distanceMap_.insert_or_assign(alertKey, 0.0);
   }

   // Update row
   if (!p->textEventKeys_.contains(alertKey))
   {
      beginInsertRows(QModelIndex(), 0, 0);
      p->textEventKeys_.push_back(alertKey);
      endInsertRows();
   }
   else
   {
      const int   row         = p->textEventKeys_.indexOf(alertKey);
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
      auto& centroid = p->centroidMap_.at(textEvent);

      if (centroid != common::Coordinate {0.0, 0.0})
      {
         p->geodesic_.Inverse(latitude,
                              longitude,
                              centroid.latitude_,
                              centroid.longitude_,
                              distanceInMeters);
         p->distanceMap_.insert_or_assign(textEvent, distanceInMeters);
      }
   }

   p->previousPosition_ = {latitude, longitude};

   QModelIndex topLeft = createIndex(0, static_cast<int>(Column::Distance));
   QModelIndex bottomRight =
      createIndex(rowCount() - 1, static_cast<int>(Column::Distance));

   emit dataChanged(topLeft, bottomRight);
}

AlertModelImpl::AlertModelImpl() :
    textEventManager_ {manager::TextEventManager::Instance()},
    textEventKeys_ {},
    geodesic_(util::GeographicLib::DefaultGeodesic()),
    distanceMap_ {},
    distanceDisplay_ {scwx::common::DistanceType::Miles},
    previousPosition_ {}
{
}

std::string AlertModelImpl::GetCounties(const types::TextEventKey& key)
{
   auto  messageList = manager::TextEventManager::Instance()->message_list(key);
   auto& lastMessage = messageList.back();
   size_t segmentCount = lastMessage->segment_count();
   auto   lastSegment  = lastMessage->segment(segmentCount - 1);
   auto   fipsIds      = lastSegment->header_->ugc_.fips_ids();

   std::vector<std::string> counties;
   counties.reserve(fipsIds.size());
   for (auto& id : fipsIds)
   {
      counties.push_back(config::CountyDatabase::GetCountyName(id));
   }
   std::sort(counties.begin(), counties.end());

   return scwx::util::ToString(counties);
}

std::string AlertModelImpl::GetState(const types::TextEventKey& key)
{
   auto  messageList = manager::TextEventManager::Instance()->message_list(key);
   auto& lastMessage = messageList.back();
   size_t segmentCount = lastMessage->segment_count();
   auto   lastSegment  = lastMessage->segment(segmentCount - 1);
   return scwx::util::ToString(lastSegment->header_->ugc_.states());
}

std::chrono::system_clock::time_point
AlertModelImpl::GetStartTime(const types::TextEventKey& key)
{
   auto  messageList = manager::TextEventManager::Instance()->message_list(key);
   auto& firstMessage = messageList.front();
   auto  firstSegment = firstMessage->segment(0);
   return firstSegment->header_->vtecString_[0].pVtec_.event_begin();
}

std::string AlertModelImpl::GetStartTimeString(const types::TextEventKey& key)
{
   return scwx::util::TimeString(GetStartTime(key));
}

std::chrono::system_clock::time_point
AlertModelImpl::GetEndTime(const types::TextEventKey& key)
{
   auto  messageList = manager::TextEventManager::Instance()->message_list(key);
   auto& lastMessage = messageList.back();
   size_t segmentCount = lastMessage->segment_count();
   auto   lastSegment  = lastMessage->segment(segmentCount - 1);
   return lastSegment->header_->vtecString_[0].pVtec_.event_end();
}

std::string AlertModelImpl::GetEndTimeString(const types::TextEventKey& key)
{
   return scwx::util::TimeString(GetEndTime(key));
}

} // namespace model
} // namespace qt
} // namespace scwx
