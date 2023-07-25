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
       static_cast<std::size_t>(index.row()) >= p->placefileNames_.size())
   {
      return QVariant();
   }

   const auto& placefileName = p->placefileNames_.at(index.row());

   if (role == Qt::ItemDataRole::DisplayRole ||
       role == Qt::ItemDataRole::ToolTipRole)
   {
      static const QString enabledString  = QObject::tr("Enabled");
      static const QString disabledString = QObject::tr("Disabled");

      static const QString thresholdsEnabledString =
         QObject::tr("Thresholds Enabled");
      static const QString thresholdsDisabledString =
         QObject::tr("Thresholds Disabled");

      switch (index.column())
      {
      case static_cast<int>(Column::Enabled):
         if (role == Qt::ItemDataRole::ToolTipRole)
         {
            return p->placefileManager_->PlacefileEnabled(placefileName) ?
                      enabledString :
                      disabledString;
         }
         break;

      case static_cast<int>(Column::Thresholds):
         if (role == Qt::ItemDataRole::ToolTipRole)
         {
            return p->placefileManager_->PlacefileThresholded(placefileName) ?
                      thresholdsEnabledString :
                      thresholdsDisabledString;
         }
         break;

      case static_cast<int>(Column::Placefile):
      {
         std::string description = placefileName;
         auto        placefile = p->placefileManager_->Placefile(placefileName);
         if (placefile != nullptr)
         {
            std::string title = placefile->title();
            if (!title.empty())
            {
               description = title + '\n' + description;
            }
         }

         return QString::fromStdString(description);
      }

      default:
         break;
      }
   }
   else if (role == types::ItemDataRole::SortRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::Enabled):
         return p->placefileManager_->PlacefileEnabled(placefileName);

      case static_cast<int>(Column::Thresholds):
         return p->placefileManager_->PlacefileThresholded(placefileName);

      case static_cast<int>(Column::Placefile):
         return QString::fromStdString(placefileName);

      default:
         break;
      }
   }
   else if (role == Qt::ItemDataRole::CheckStateRole)
   {
      switch (index.column())
      {
      case static_cast<int>(Column::Enabled):
         return static_cast<int>(
            p->placefileManager_->PlacefileEnabled(placefileName) ?
               Qt::CheckState::Checked :
               Qt::CheckState::Unchecked);

      case static_cast<int>(Column::Thresholds):
         return static_cast<int>(
            p->placefileManager_->PlacefileThresholded(placefileName) ?
               Qt::CheckState::Checked :
               Qt::CheckState::Unchecked);

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
