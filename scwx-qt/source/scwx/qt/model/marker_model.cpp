#include <scwx/qt/model/marker_model.hpp>
#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/qt/types/marker_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <QApplication>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::marker_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn =
   static_cast<int>(MarkerModel::Column::Latitude);
static constexpr int kLastColumn =
   static_cast<int>(MarkerModel::Column::Name);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

class MarkerModel::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;
   std::shared_ptr<manager::MarkerManager> markerManager_ {
      manager::MarkerManager::Instance()};
};

MarkerModel::MarkerModel(QObject* parent) :
   QAbstractTableModel(parent), p(std::make_unique<Impl>())
{

   connect(p->markerManager_.get(),
         &manager::MarkerManager::MarkersInitialized,
         this,
         &MarkerModel::HandleMarkersInitialized);

   connect(p->markerManager_.get(),
         &manager::MarkerManager::MarkerAdded,
         this,
         &MarkerModel::HandleMarkerAdded);

   connect(p->markerManager_.get(),
         &manager::MarkerManager::MarkerChanged,
         this,
         &MarkerModel::HandleMarkerChanged);

   connect(p->markerManager_.get(),
         &manager::MarkerManager::MarkerRemoved,
         this,
         &MarkerModel::HandleMarkerRemoved);
}

MarkerModel::~MarkerModel() = default;

int MarkerModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ?
             0 :
             static_cast<int>(p->markerManager_->marker_count());
}

int MarkerModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : kNumColumns;
}

Qt::ItemFlags MarkerModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flags = QAbstractTableModel::flags(index);

   switch (index.column())
   {
   case static_cast<int>(Column::Name):
   case static_cast<int>(Column::Latitude):
   case static_cast<int>(Column::Longitude):
      flags |= Qt::ItemFlag::ItemIsEditable;
      break;
   default:
      break;
   }

   return flags;
}

QVariant MarkerModel::data(const QModelIndex& index, int role) const
{

   static const char COORDINATE_FORMAT    = 'g';
   static const int  COORDINATE_PRECISION = 6;

   if (!index.isValid() || index.row() < 0)
   {
      return QVariant();
   }

   std::optional<types::MarkerInfo> markerInfo =
      p->markerManager_->get_marker(index.row());
   if (!markerInfo)
   {
      return QVariant();
   }

   switch(index.column())
   {
   case static_cast<int>(Column::Name):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole ||
          role == Qt::ItemDataRole::EditRole)
      {
         return QString::fromStdString(markerInfo->name);
      }
      break;

   case static_cast<int>(Column::Latitude):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole ||
          role == Qt::ItemDataRole::EditRole)
      {
         return QString::number(
            markerInfo->latitude, COORDINATE_FORMAT, COORDINATE_PRECISION);
      }
      break;

   case static_cast<int>(Column::Longitude):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole ||
          role == Qt::ItemDataRole::EditRole)
      {
         return QString::number(
            markerInfo->longitude, COORDINATE_FORMAT, COORDINATE_PRECISION);
      }
      break;

   default:
      break;
   }

   return QVariant();
}

QVariant MarkerModel::headerData(int             section,
                                 Qt::Orientation orientation,
                                 int             role) const
{
   if (role == Qt::ItemDataRole::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
            case static_cast<int>(Column::Name):
               return tr("Name");

            case static_cast<int>(Column::Latitude):
               return tr("Latitude");

            case static_cast<int>(Column::Longitude):
               return tr("Longitude");

            default:
               break;
         }
      }
   }

   return QVariant();
}

bool MarkerModel::setData(const QModelIndex& index,
                          const QVariant&    value,
                          int                role)
{
   if (!index.isValid() || index.row() < 0)
   {
      return false;
   }
   std::optional<types::MarkerInfo> markerInfo =
      p->markerManager_->get_marker(index.row());
   if (!markerInfo)
   {
      return false;
   }
   bool result = false;

   switch(index.column())
   {
   case static_cast<int>(Column::Name):
      if (role == Qt::ItemDataRole::EditRole)
      {
         QString str = value.toString();
         markerInfo->name = str.toStdString();
         p->markerManager_->set_marker(index.row(), *markerInfo);
         result = true;
      }
      break;

   case static_cast<int>(Column::Latitude):
      if (role == Qt::ItemDataRole::EditRole)
      {
         QString str = value.toString();
         bool ok;
         double latitude = str.toDouble(&ok);
         if (!str.isEmpty() && ok && -90 <= latitude && latitude <= 90)
         {
            markerInfo->latitude = latitude;
            p->markerManager_->set_marker(index.row(), *markerInfo);
            result = true;
         }
      }
      break;

   case static_cast<int>(Column::Longitude):
      if (role == Qt::ItemDataRole::EditRole)
      {
         QString str = value.toString();
         bool ok;
         double longitude = str.toDouble(&ok);
         if (!str.isEmpty() && ok && -180 <= longitude && longitude <= 180)
         {
            markerInfo->longitude = longitude;
            p->markerManager_->set_marker(index.row(), *markerInfo);
            result = true;
         }
      }
      break;

   default:
      break;
   }

   if (result)
   {
      Q_EMIT dataChanged(index, index);
   }

   return result;
}

void MarkerModel::HandleMarkersInitialized(size_t count)
{
   QModelIndex topLeft = createIndex(0, kFirstColumn);
   QModelIndex bottomRight = createIndex(count - 1, kLastColumn);

   beginInsertRows(QModelIndex(), 0, count - 1);
   endInsertRows();

   Q_EMIT dataChanged(topLeft, bottomRight);
}

void MarkerModel::HandleMarkerAdded()
{
   const int newIndex = static_cast<int>(p->markerManager_->marker_count() - 1);
   QModelIndex topLeft = createIndex(newIndex, kFirstColumn);
   QModelIndex bottomRight = createIndex(newIndex, kLastColumn);

   beginInsertRows(QModelIndex(), newIndex, newIndex);
   endInsertRows();

   Q_EMIT dataChanged(topLeft, bottomRight);
}

void MarkerModel::HandleMarkerChanged(size_t index)
{
   const int changedIndex = static_cast<int>(index);
   QModelIndex topLeft = createIndex(changedIndex, kFirstColumn);
   QModelIndex bottomRight = createIndex(changedIndex, kLastColumn);

   Q_EMIT dataChanged(topLeft, bottomRight);
}

void MarkerModel::HandleMarkerRemoved(size_t index)
{
   const int removedIndex = static_cast<int>(index);
   QModelIndex topLeft = createIndex(removedIndex, kFirstColumn);
   QModelIndex bottomRight = createIndex(removedIndex, kLastColumn);

   beginRemoveRows(QModelIndex(), removedIndex, removedIndex);
   endRemoveRows();

   Q_EMIT dataChanged(topLeft, bottomRight);
}

} // namespace model
} // namespace qt
} // namespace scwx
