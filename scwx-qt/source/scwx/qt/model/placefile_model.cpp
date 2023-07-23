#include <scwx/qt/model/placefile_model.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::placefile_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn =
   static_cast<int>(PlacefileModel::Column::Enabled);
static constexpr int kLastColumn =
   static_cast<int>(PlacefileModel::Column::Description);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

class PlacefileModelImpl
{
public:
   explicit PlacefileModelImpl() {}
   ~PlacefileModelImpl() = default;

   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};

   std::vector<std::string> placefileNames_ {};
};

PlacefileModel::PlacefileModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<PlacefileModelImpl>())
{
   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileUpdated,
           this,
           &PlacefileModel::HandlePlacefileUpdate);
}
PlacefileModel::~PlacefileModel() = default;

int PlacefileModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : static_cast<int>(p->placefileNames_.size());
}

int PlacefileModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : kNumColumns;
}

Qt::ItemFlags PlacefileModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flags = QAbstractTableModel::flags(index);

   switch (index.column())
   {
   case static_cast<int>(Column::Enabled):
   case static_cast<int>(Column::Thresholds):
      flags |= Qt::ItemFlag::ItemIsUserCheckable;

   default:
      break;
   }

   return flags;
}

QVariant PlacefileModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 ||
       index.row() >= p->placefileNames_.size())
   {
      return QVariant();
   }

   const auto& placefileName = p->placefileNames_.at(index.row());

   if (role == Qt::ItemDataRole::DisplayRole ||
       role == types::ItemDataRole::SortRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::Enabled):
         if (role == types::ItemDataRole::SortRole)
         {
            return true; // TODO
         }
         break;

      case static_cast<int>(Column::Thresholds):
         if (role == types::ItemDataRole::SortRole)
         {
            return true; // TODO
         }
         break;

      case static_cast<int>(Column::Url):
         return QString::fromStdString(placefileName);

      case static_cast<int>(Column::Description):
         return QString {};

      default:
         break;
      }
   }
   else if (role == Qt::ItemDataRole::CheckStateRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::Enabled):
         return true; // TODO

      case static_cast<int>(Column::Thresholds):
         return true; // TODO

      default:
         break;
      }
   }

   return QVariant();
}

QVariant PlacefileModel::headerData(int             section,
                                    Qt::Orientation orientation,
                                    int             role) const
{
   if (role == Qt::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
         case static_cast<int>(Column::Enabled):
            return tr("Enabled");

         case static_cast<int>(Column::Thresholds):
            return tr("Thresholds");

         case static_cast<int>(Column::Url):
            return tr("URL");

         case static_cast<int>(Column::Description):
            return tr("Description");

         default:
            break;
         }
      }
   }

   return QVariant();
}

void PlacefileModel::HandlePlacefileUpdate(const std::string& name)
{
   auto it =
      std::find(p->placefileNames_.begin(), p->placefileNames_.end(), name);

   if (it != p->placefileNames_.end())
   {
      // Placefile exists, mark row as updated
      const int   row         = std::distance(p->placefileNames_.begin(), it);
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile is new, append row
      const int newIndex = static_cast<int>(p->placefileNames_.size());
      beginInsertRows(QModelIndex(), newIndex, newIndex);
      p->placefileNames_.push_back(name);
      endInsertRows();
   }
}

} // namespace model
} // namespace qt
} // namespace scwx
