#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/strings.hpp>
#include <scwx/util/time.hpp>

#include <QApplication>
#include <QFontMetrics>

namespace scwx::qt::model
{

static const std::string logPrefix_ = "scwx::qt::model::alert_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn = static_cast<int>(AlertModel::Column::Etn);
static constexpr int kLastColumn =
   static_cast<int>(AlertModel::Column::Distance);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

class AlertModel::Impl
{
public:
   explicit Impl(AlertModel* self);
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   bool                       GetObserved(const types::TextEventKey& key);
   awips::ibw::ThreatCategory GetThreatCategory(const types::TextEventKey& key);
   bool GetTornadoPossible(const types::TextEventKey& key);

   static std::string GetCounties(const types::TextEventKey& key);
   static std::string GetState(const types::TextEventKey& key);
   static std::chrono::system_clock::time_point
                      GetStartTime(const types::TextEventKey& key);
   static std::string GetStartTimeString(const types::TextEventKey& key);
   static std::chrono::system_clock::time_point
                      GetEndTime(const types::TextEventKey& key);
   static std::string GetEndTimeString(const types::TextEventKey& key);

   void TimeFormatChanged();

   AlertModel* self_ {nullptr};

   boost::signals2::scoped_connection currentTimeZoneConnection_ {};
   boost::signals2::scoped_connection defaultClockFormatConnection_ {};

   std::shared_ptr<manager::TextEventManager> textEventManager_;

   QList<types::TextEventKey> textEventKeys_;

   const GeographicLib::Geodesic& geodesic_;

   std::unordered_map<types::TextEventKey,
                      bool,
                      types::TextEventHash<types::TextEventKey>>
      observedMap_;
   std::unordered_map<types::TextEventKey,
                      awips::ibw::ThreatCategory,
                      types::TextEventHash<types::TextEventKey>>
      threatCategoryMap_;
   std::unordered_map<types::TextEventKey,
                      bool,
                      types::TextEventHash<types::TextEventKey>>
      tornadoPossibleMap_;
   std::unordered_map<types::TextEventKey,
                      common::Coordinate,
                      types::TextEventHash<types::TextEventKey>>
      centroidMap_;
   std::unordered_map<types::TextEventKey,
                      double,
                      types::TextEventHash<types::TextEventKey>>
                            distanceMap_;
   scwx::common::Coordinate previousPosition_;
};

AlertModel::AlertModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<Impl>(this))
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

      case static_cast<int>(Column::Tornado):
         if (textEventKey.phenomenon_ == awips::Phenomenon::Tornado &&
             p->GetObserved(textEventKey))
         {
            return tr("Observed");
         }
         if (p->GetTornadoPossible(textEventKey))
         {
            return tr("Possible");
         }
         break;

      case static_cast<int>(Column::ThreatCategory):
         if (role == Qt::DisplayRole)
         {
            return QString::fromStdString(awips::ibw::GetThreatCategoryName(
               p->GetThreatCategory(textEventKey)));
         }
         else
         {
            return static_cast<int>(p->GetThreatCategory(textEventKey));
         }

      case static_cast<int>(Column::State):
         return QString::fromStdString(Impl::GetState(textEventKey));

      case static_cast<int>(Column::Counties):
         return QString::fromStdString(Impl::GetCounties(textEventKey));

      case static_cast<int>(Column::StartTime):
         return QString::fromStdString(Impl::GetStartTimeString(textEventKey));

      case static_cast<int>(Column::EndTime):
         return QString::fromStdString(Impl::GetEndTimeString(textEventKey));

      case static_cast<int>(Column::Distance):
         if (role == Qt::DisplayRole)
         {
            const std::string distanceUnitName =
               settings::UnitSettings::Instance().distance_units().GetValue();
            types::DistanceUnits distanceUnits =
               types::GetDistanceUnitsFromName(distanceUnitName);
            double distanceScale = types::GetDistanceUnitsScale(distanceUnits);
            std::string abbreviation =
               types::GetDistanceUnitsAbbreviation(distanceUnits);

            return QString("%1 %2")
               .arg(static_cast<uint32_t>(p->distanceMap_.at(textEventKey) *
                                          scwx::common::kKilometersPerMeter *
                                          distanceScale))
               .arg(QString::fromStdString(abbreviation));
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
         return QVariant::fromValue(Impl::GetStartTime(textEventKey));

      case static_cast<int>(Column::EndTime):
         return QVariant::fromValue(Impl::GetEndTime(textEventKey));

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

         case static_cast<int>(Column::ThreatCategory):
            return tr("Category");

         case static_cast<int>(Column::Tornado):
            return tr("Tornado");

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

      case static_cast<int>(Column::ThreatCategory):
         contentsSize = fontMetrics.size(0, QString(6, 'W'));
         break;

      case static_cast<int>(Column::Tornado):
         contentsSize = fontMetrics.size(0, QString(5, 'W'));
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
                             std::size_t                messageIndex,
                             boost::uuids::uuid         uuid)
{
   logger_->trace("Handle alert: {}", alertKey.ToString());

   double distanceInMeters;

   const auto& alertMessages = p->textEventManager_->message_list(alertKey);

   // Find message by UUID instead of index, as the message index could have
   // changed between the signal being emitted and the handler being called
   auto messageIt = std::find_if(alertMessages.cbegin(),
                                 alertMessages.cend(),
                                 [&uuid](const auto& message)
                                 { return uuid == message->uuid(); });

   if (messageIt == alertMessages.cend())
   {
      logger_->warn("Could not find alert uuid: {} ({})",
                    alertKey.ToString(),
                    messageIndex);
      return;
   }

   auto& message = *messageIt;

   // Store the current message index
   messageIndex = static_cast<std::size_t>(
      std::distance(alertMessages.cbegin(), messageIt));

   // Skip alert if this is not the most recent message
   if (messageIndex + 1 < alertMessages.size())
   {
      return;
   }

   // Get the most recent segment for the event
   const std::shared_ptr<const awips::Segment> alertSegment =
      message->segments().back();

   p->observedMap_.insert_or_assign(alertKey, alertSegment->observed_);
   p->threatCategoryMap_.insert_or_assign(alertKey,
                                          alertSegment->threatCategory_);
   p->tornadoPossibleMap_.insert_or_assign(alertKey,
                                           alertSegment->tornadoPossible_);

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
      int newIndex = p->textEventKeys_.size();
      beginInsertRows(QModelIndex(), newIndex, newIndex);
      p->textEventKeys_.push_back(alertKey);
      endInsertRows();
   }
   else
   {
      const int   row         = p->textEventKeys_.indexOf(alertKey);
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
}

void AlertModel::HandleAlertsRemoved(
   const std::unordered_set<types::TextEventKey,
                            types::TextEventHash<types::TextEventKey>>&
      alertKeys)
{
   logger_->trace("Handle alerts removed");

   for (const auto& alertKey : alertKeys)
   {
      // Remove from the list of text event keys
      auto it = std::find(
         p->textEventKeys_.begin(), p->textEventKeys_.end(), alertKey);
      if (it != p->textEventKeys_.end())
      {
         const int row =
            static_cast<int>(std::distance(p->textEventKeys_.begin(), it));
         beginRemoveRows(QModelIndex(), row, row);
         p->textEventKeys_.erase(it);
         endRemoveRows();
      }

      // Remove from internal maps
      p->observedMap_.erase(alertKey);
      p->threatCategoryMap_.erase(alertKey);
      p->tornadoPossibleMap_.erase(alertKey);
      p->centroidMap_.erase(alertKey);
      p->distanceMap_.erase(alertKey);
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

   const int rows = rowCount();
   if (rows > 0)
   {
      const auto topLeft = createIndex(0, static_cast<int>(Column::Distance));
      const auto bottomRight =
         createIndex(rows - 1, static_cast<int>(Column::Distance));

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
}

AlertModel::Impl::Impl(AlertModel* self) :
    self_ {self},
    textEventManager_ {manager::TextEventManager::Instance()},
    textEventKeys_ {},
    geodesic_(util::GeographicLib::DefaultGeodesic()),
    distanceMap_ {},
    previousPosition_ {}
{
   currentTimeZoneConnection_ =
      scwx::util::time::current_time_zone_changed_signal().connect(
         [this](const scwx::util::time::time_zone*) { TimeFormatChanged(); });
   defaultClockFormatConnection_ =
      scwx::util::time::default_clock_format_changed_signal().connect(
         [this](scwx::util::ClockFormat) { TimeFormatChanged(); });
}

bool AlertModel::Impl::GetObserved(const types::TextEventKey& key)
{
   bool observed = false;

   auto it = observedMap_.find(key);
   if (it != observedMap_.cend())
   {
      observed = it->second;
   }

   return observed;
}

awips::ibw::ThreatCategory
AlertModel::Impl::GetThreatCategory(const types::TextEventKey& key)
{
   awips::ibw::ThreatCategory threatCategory = awips::ibw::ThreatCategory::Base;

   auto it = threatCategoryMap_.find(key);
   if (it != threatCategoryMap_.cend())
   {
      threatCategory = it->second;
   }

   return threatCategory;
}

bool AlertModel::Impl::GetTornadoPossible(const types::TextEventKey& key)
{
   bool tornadoPossible = false;

   auto it = tornadoPossibleMap_.find(key);
   if (it != tornadoPossibleMap_.cend())
   {
      tornadoPossible = it->second;
   }

   return tornadoPossible;
}

std::string AlertModel::Impl::GetCounties(const types::TextEventKey& key)
{
   auto messageList = manager::TextEventManager::Instance()->message_list(key);

   if (messageList.size() > 0)
   {
      auto&  lastMessage  = messageList.back();
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
   else
   {
      logger_->trace("GetCounties(): No message associated with key: {}",
                     key.ToString());
      return {};
   }
}

std::string AlertModel::Impl::GetState(const types::TextEventKey& key)
{
   auto messageList = manager::TextEventManager::Instance()->message_list(key);

   if (messageList.size() > 0)
   {
      auto&  lastMessage  = messageList.back();
      size_t segmentCount = lastMessage->segment_count();
      auto   lastSegment  = lastMessage->segment(segmentCount - 1);
      return scwx::util::ToString(lastSegment->header_->ugc_.states());
   }
   else
   {
      logger_->trace("GetState(): No message associated with key: {}",
                     key.ToString());
      return {};
   }
}

std::chrono::system_clock::time_point
AlertModel::Impl::GetStartTime(const types::TextEventKey& key)
{
   auto messageList = manager::TextEventManager::Instance()->message_list(key);

   if (messageList.size() > 0)
   {
      auto& firstMessage = messageList.front();
      return firstMessage->segment_event_begin(0);
   }
   else
   {
      logger_->trace("GetStartTime(): No message associated with key: {}",
                     key.ToString());
      return {};
   }
}

std::string AlertModel::Impl::GetStartTimeString(const types::TextEventKey& key)
{
   return scwx::util::TimeString(GetStartTime(key),
                                 scwx::util::time::ClockFormat::Default,
                                 scwx::util::time::current_time_zone());
}

std::chrono::system_clock::time_point
AlertModel::Impl::GetEndTime(const types::TextEventKey& key)
{
   auto messageList = manager::TextEventManager::Instance()->message_list(key);

   if (messageList.size() > 0)
   {
      auto&  lastMessage  = messageList.back();
      size_t segmentCount = lastMessage->segment_count();
      auto   lastSegment  = lastMessage->segment(segmentCount - 1);
      return lastSegment->header_->vtecString_[0].pVtec_.event_end();
   }
   else
   {
      logger_->trace("GetEndTime(): No message associated with key: {}",
                     key.ToString());
      return {};
   }
}

std::string AlertModel::Impl::GetEndTimeString(const types::TextEventKey& key)
{
   return scwx::util::TimeString(GetEndTime(key),
                                 scwx::util::time::ClockFormat::Default,
                                 scwx::util::time::current_time_zone());
}

void AlertModel::Impl::TimeFormatChanged()
{
   logger_->trace("TimeFormatChanged()");

   const int rowCount = self_->rowCount();
   if (rowCount > 0)
   {
      const auto topLeft =
         self_->createIndex(0, static_cast<int>(Column::StartTime));
      const auto bottomRight =
         self_->createIndex(rowCount - 1, static_cast<int>(Column::EndTime));

      Q_EMIT self_->dataChanged(topLeft, bottomRight);
   }
}

} // namespace scwx::qt::model
