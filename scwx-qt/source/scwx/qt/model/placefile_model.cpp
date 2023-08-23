#include <scwx/qt/model/placefile_model.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QStyle>
#include <QStyleOption>

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
   static_cast<int>(PlacefileModel::Column::Placefile);
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
           &manager::PlacefileManager::PlacefileEnabled,
           this,
           &PlacefileModel::HandlePlacefileUpdate);

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileRemoved,
           this,
           &PlacefileModel::HandlePlacefileRemoved);

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileRenamed,
           this,
           &PlacefileModel::HandlePlacefileRenamed);

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
      flags |= Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEditable;
      break;

   case static_cast<int>(Column::Placefile):
      flags |= Qt::ItemFlag::ItemIsEditable;
      break;

   default:
      break;
   }

   return flags;
}

QVariant PlacefileModel::data(const QModelIndex& index, int role) const
{
   static const QString enabledString  = QObject::tr("Enabled");
   static const QString disabledString = QObject::tr("Disabled");

   static const QString thresholdsEnabledString =
      QObject::tr("Thresholds Enabled");
   static const QString thresholdsDisabledString =
      QObject::tr("Thresholds Disabled");

   if (!index.isValid() || index.row() < 0 ||
       static_cast<std::size_t>(index.row()) >= p->placefileNames_.size())
   {
      return QVariant();
   }

   const auto& placefileName = p->placefileNames_.at(index.row());

   switch (index.column())
   {
   case static_cast<int>(Column::Enabled):
      if (role == Qt::ItemDataRole::ToolTipRole)
      {
         return p->placefileManager_->placefile_enabled(placefileName) ?
                   enabledString :
                   disabledString;
      }
      else if (role == Qt::ItemDataRole::CheckStateRole)
      {
         return static_cast<int>(
            p->placefileManager_->placefile_enabled(placefileName) ?
               Qt::CheckState::Checked :
               Qt::CheckState::Unchecked);
      }
      else if (role == types::ItemDataRole::SortRole)
      {
         return p->placefileManager_->placefile_enabled(placefileName);
      }
      break;

   case static_cast<int>(Column::Thresholds):
      if (role == Qt::ItemDataRole::ToolTipRole)
      {
         return p->placefileManager_->placefile_thresholded(placefileName) ?
                   thresholdsEnabledString :
                   thresholdsDisabledString;
      }
      else if (role == Qt::ItemDataRole::CheckStateRole)
      {
         return static_cast<int>(
            p->placefileManager_->placefile_thresholded(placefileName) ?
               Qt::CheckState::Checked :
               Qt::CheckState::Unchecked);
      }
      else if (role == types::ItemDataRole::SortRole)
      {
         return p->placefileManager_->placefile_thresholded(placefileName);
      }
      break;

   case static_cast<int>(Column::Placefile):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         std::string description = placefileName;
         std::string title =
            p->placefileManager_->placefile_title(placefileName);
         if (!title.empty())
         {
            description = title + '\n' + description;
         }

         return QString::fromStdString(description);
      }
      else if (role == Qt::ItemDataRole::EditRole ||
               role == types::ItemDataRole::SortRole)
      {
         return QString::fromStdString(placefileName);
      }
      break;

   default:
      break;
   }

   return QVariant();
}

QVariant PlacefileModel::headerData(int             section,
                                    Qt::Orientation orientation,
                                    int             role) const
{
   if (role == Qt::ItemDataRole::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
         case static_cast<int>(Column::Enabled):
            return tr("E");

         case static_cast<int>(Column::Thresholds):
            return tr("T");

         case static_cast<int>(Column::Placefile):
            return tr("Placefile");

         default:
            break;
         }
      }
   }
   else if (role == Qt::ItemDataRole::ToolTipRole)
   {
      switch (section)
      {
      case static_cast<int>(Column::Enabled):
         return tr("Enabled");

      case static_cast<int>(Column::Thresholds):
         return tr("Thresholds");

      default:
         break;
      }
   }
   else if (role == Qt::ItemDataRole::SizeHintRole)
   {
      switch (section)
      {
      case static_cast<int>(Column::Enabled):
      case static_cast<int>(Column::Thresholds):
      {
         static const QCheckBox checkBox {};
         QStyleOptionButton     option {};
         option.initFrom(&checkBox);

         // Width values from QCheckBox
         return QApplication::style()->sizeFromContents(
            QStyle::ContentsType::CT_CheckBox,
            &option,
            {option.iconSize.width() + 4, 0});
      }

      default:
         break;
      }
   }

   return QVariant();
}

bool PlacefileModel::setData(const QModelIndex& index,
                             const QVariant&    value,
                             int                role)
{
   if (!index.isValid() || index.row() < 0 ||
       static_cast<std::size_t>(index.row()) >= p->placefileNames_.size())
   {
      return false;
   }

   const auto& placefileName = p->placefileNames_.at(index.row());

   switch (index.column())
   {
   case static_cast<int>(Column::Enabled):
      if (role == Qt::ItemDataRole::CheckStateRole)
      {
         p->placefileManager_->set_placefile_enabled(placefileName,
                                                     value.toBool());
         return true;
      }
      break;

   case static_cast<int>(Column::Thresholds):
      if (role == Qt::ItemDataRole::CheckStateRole)
      {
         p->placefileManager_->set_placefile_thresholded(placefileName,
                                                         value.toBool());
         return true;
      }
      break;

   case static_cast<int>(Column::Placefile):
      if (role == Qt::ItemDataRole::EditRole)
      {
         p->placefileManager_->set_placefile_url(
            placefileName, value.toString().toStdString());
         return true;
      }
      break;

   default:
      break;
   }

   return true;
}

void PlacefileModel::HandlePlacefileRemoved(const std::string& name)
{
   auto it =
      std::find(p->placefileNames_.begin(), p->placefileNames_.end(), name);

   if (it != p->placefileNames_.end())
   {
      // Placefile exists, delete row
      const int row = std::distance(p->placefileNames_.begin(), it);

      beginRemoveRows(QModelIndex(), row, row);
      p->placefileNames_.erase(it);
      endRemoveRows();
   }
}

void PlacefileModel::HandlePlacefileRenamed(const std::string& oldName,
                                            const std::string& newName)
{
   auto it =
      std::find(p->placefileNames_.begin(), p->placefileNames_.end(), oldName);

   if (it != p->placefileNames_.end())
   {
      // Placefile exists, mark row as updated
      const int   row         = std::distance(p->placefileNames_.begin(), it);
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      // Rename placefile
      *it = newName;

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile is new, append row
      const int newIndex = static_cast<int>(p->placefileNames_.size());
      beginInsertRows(QModelIndex(), newIndex, newIndex);
      p->placefileNames_.push_back(newName);
      endInsertRows();
   }
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
