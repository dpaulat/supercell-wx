#include <scwx/common/geographic.hpp>
#include <scwx/qt/model/marker_model.hpp>
#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/qt/types/marker_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/util/q_color_modulate.hpp>
#include <scwx/util/logger.hpp>

#include <vector>

#include <QApplication>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::marker_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);
static const int         iconSize_  = 30;

static constexpr int kFirstColumn =
   static_cast<int>(MarkerModel::Column::Latitude);
static constexpr int kLastColumn = static_cast<int>(MarkerModel::Column::Name);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

class MarkerModel::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;
   std::shared_ptr<manager::MarkerManager> markerManager_ {
      manager::MarkerManager::Instance()};
   std::vector<types::MarkerId> markerIds_;
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
   return parent.isValid() ? 0 : static_cast<int>(p->markerIds_.size());
}

int MarkerModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : kNumColumns;
}

Qt::ItemFlags MarkerModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flags = QAbstractTableModel::flags(index);

   return flags;
}

QVariant MarkerModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 ||
       static_cast<size_t>(index.row()) >= p->markerIds_.size())
   {
      logger_->debug("Failed to get data index {}", index.row());
      return QVariant();
   }

   types::MarkerId                  id = p->markerIds_[index.row()];
   std::optional<types::MarkerInfo> markerInfo =
      p->markerManager_->get_marker(id);
   if (!markerInfo)
   {
      logger_->debug("Failed to get data index {} id {}", index.row(), id);
      return QVariant();
   }

   if (role == Qt::ItemDataRole::UserRole)
   {
      return qulonglong(id);
   }

   switch (index.column())
   {
   case static_cast<int>(Column::Name):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         return QString::fromStdString(markerInfo->name);
      }
      break;

   case static_cast<int>(Column::Latitude):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         return QString::fromStdString(
            common::GetLatitudeString(markerInfo->latitude));
      }
      break;

   case static_cast<int>(Column::Longitude):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         return QString::fromStdString(
            common::GetLongitudeString(markerInfo->longitude));
      }
      break;
      break;
   case static_cast<int>(Column::Icon):
      if (role == Qt::ItemDataRole::DisplayRole)
      {
         std::optional<types::MarkerIconInfo> icon =
            p->markerManager_->get_icon(markerInfo->iconName);
         if (icon)
         {
            return QString::fromStdString(icon->shortName);
         }
         else
         {
            return {};
         }
      }
      else if (role == Qt::ItemDataRole::DecorationRole)
      {
         std::optional<types::MarkerIconInfo> icon =
            p->markerManager_->get_icon(markerInfo->iconName);
         if (icon)
         {
            return util::modulateColors(icon->qIcon,
                                        QSize(iconSize_, iconSize_),
                                        QColor(markerInfo->iconColor[0],
                                               markerInfo->iconColor[1],
                                               markerInfo->iconColor[2],
                                               markerInfo->iconColor[3]));
         }
         else
         {
            return {};
         }
      }
      break;

   default:
      break;
   }

   return QVariant();
}

std::optional<types::MarkerId> MarkerModel::getId(int index)
{
   if (index < 0 || static_cast<size_t>(index) >= p->markerIds_.size())
   {
      return {};
   }

   return p->markerIds_[index];
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

         case static_cast<int>(Column::Icon):
            return tr("Icon");

         default:
            break;
         }
      }
   }

   return QVariant();
}

bool MarkerModel::setData(const QModelIndex&, const QVariant&, int)
{
   return false;
}

void MarkerModel::HandleMarkersInitialized(size_t count)
{
   if (count == 0)
   {
      return;
   }

   beginResetModel();
   p->markerIds_.clear();
   p->markerIds_.reserve(count);
   p->markerManager_->for_each([this](const types::MarkerInfo& info)
                               { p->markerIds_.push_back(info.id); });
   endResetModel();
}

void MarkerModel::HandleMarkerAdded(types::MarkerId id)
{
   std::optional<size_t> index = p->markerManager_->get_index(id);
   if (!index)
   {
      return;
   }
   const int newIndex = static_cast<int>(*index);

   beginInsertRows(QModelIndex(), newIndex, newIndex);
   auto it = std::next(p->markerIds_.begin(), newIndex);
   p->markerIds_.emplace(it, id);
   endInsertRows();
}

void MarkerModel::HandleMarkerChanged(types::MarkerId id)
{
   auto it = std::find(p->markerIds_.begin(), p->markerIds_.end(), id);
   if (it == p->markerIds_.end())
   {
      return;
   }
   const int changedIndex = std::distance(p->markerIds_.begin(), it);

   const QModelIndex topLeft     = createIndex(changedIndex, kFirstColumn);
   const QModelIndex bottomRight = createIndex(changedIndex, kLastColumn);

   Q_EMIT dataChanged(topLeft, bottomRight);
}

void MarkerModel::HandleMarkerRemoved(types::MarkerId id)
{
   auto it = std::find(p->markerIds_.begin(), p->markerIds_.end(), id);
   if (it == p->markerIds_.end())
   {
      return;
   }

   const int removedIndex = std::distance(p->markerIds_.begin(), it);

   beginRemoveRows(QModelIndex(), removedIndex, removedIndex);
   p->markerIds_.erase(it);
   endRemoveRows();
}

} // namespace model
} // namespace qt
} // namespace scwx
