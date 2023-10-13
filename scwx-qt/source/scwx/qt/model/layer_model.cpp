#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <variant>

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QStyle>
#include <QStyleOption>
#include <boost/container/devector.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::layer_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn = static_cast<int>(LayerModel::Column::Order);
static constexpr int kLastColumn =
   static_cast<int>(LayerModel::Column::Description);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

static const std::unordered_map<LayerModel::LayerType, std::string>
   layerTypeNames_ {{LayerModel::LayerType::Map, "Map"},
                    {LayerModel::LayerType::Radar, "Radar"},
                    {LayerModel::LayerType::Alert, "Alert"},
                    {LayerModel::LayerType::Placefile, "Placefile"}};

typedef std::variant<std::monostate, std::string, awips::Phenomenon>
   LayerDescription;
typedef boost::container::devector<
   std::pair<LayerModel::LayerType, LayerDescription>>
   LayerVector;

class LayerModel::Impl
{
public:
   explicit Impl()
   {
      layers_.emplace_back(LayerType::Alert, awips::Phenomenon::Tornado);
      layers_.emplace_back(LayerType::Alert, awips::Phenomenon::SnowSquall);
      layers_.emplace_back(LayerType::Alert,
                           awips::Phenomenon::SevereThunderstorm);
      layers_.emplace_back(LayerType::Alert, awips::Phenomenon::FlashFlood);
      layers_.emplace_back(LayerType::Alert, awips::Phenomenon::Marine);
      layers_.emplace_back(LayerType::Map, "Map Overlay");
      layers_.emplace_back(LayerType::Radar, std::monostate {});
      layers_.emplace_back(LayerType::Map, "Map Underlay");
   }
   ~Impl() = default;

   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};

   LayerVector layers_ {};
};

LayerModel::LayerModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<Impl>())
{
   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileEnabled,
           this,
           &LayerModel::HandlePlacefileUpdate);

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileRemoved,
           this,
           &LayerModel::HandlePlacefileRemoved);

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileRenamed,
           this,
           &LayerModel::HandlePlacefileRenamed);

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileUpdated,
           this,
           &LayerModel::HandlePlacefileUpdate);
}
LayerModel::~LayerModel() = default;

int LayerModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : static_cast<int>(p->layers_.size());
}

int LayerModel::columnCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : kNumColumns;
}

Qt::ItemFlags LayerModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flags = QAbstractTableModel::flags(index);

   switch (index.column())
   {
   case static_cast<int>(Column::DisplayMap1):
   case static_cast<int>(Column::DisplayMap2):
   case static_cast<int>(Column::DisplayMap3):
   case static_cast<int>(Column::DisplayMap4):
      flags |= Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEditable;
      break;

   default:
      break;
   }

   return flags;
}

QVariant LayerModel::data(const QModelIndex& index, int role) const
{
   static const QString enabledString  = QObject::tr("Enabled");
   static const QString disabledString = QObject::tr("Disabled");

   static const QString displayedString = QObject::tr("Displayed");
   static const QString hiddenString    = QObject::tr("Hidden");

   if (!index.isValid() || index.row() < 0 ||
       static_cast<std::size_t>(index.row()) >= p->layers_.size())
   {
      return QVariant();
   }

   const auto& layer   = p->layers_.at(index.row());
   bool        enabled = true; // TODO

   switch (index.column())
   {
   case static_cast<int>(Column::Order):
      if (role == Qt::ItemDataRole::DisplayRole)
      {
         return index.row() + 1;
      }
      break;

   case static_cast<int>(Column::DisplayMap1):
   case static_cast<int>(Column::DisplayMap2):
   case static_cast<int>(Column::DisplayMap3):
   case static_cast<int>(Column::DisplayMap4):
      // TODO
      if (role == Qt::ItemDataRole::ToolTipRole)
      {
         return enabled ? displayedString : hiddenString;
      }
      else if (role == Qt::ItemDataRole::CheckStateRole)
      {
         return static_cast<int>(enabled ? Qt::CheckState::Checked :
                                           Qt::CheckState::Unchecked);
      }
      break;

   case static_cast<int>(Column::Type):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         return QString::fromStdString(layerTypeNames_.at(layer.first));
      }
      break;

   case static_cast<int>(Column::Enabled):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         if (layer.first == LayerType::Placefile)
         {
            return p->placefileManager_->placefile_enabled(
                      std::get<std::string>(layer.second)) ?
                      enabledString :
                      disabledString;
         }
         else
         {
            return enabledString;
         }
      }
      break;

   case static_cast<int>(Column::Description):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         if (layer.first == LayerType::Placefile)
         {
            std::string placefileName = std::get<std::string>(layer.second);
            std::string description   = placefileName;
            std::string title =
               p->placefileManager_->placefile_title(placefileName);
            if (!title.empty())
            {
               description = title + '\n' + description;
            }

            return QString::fromStdString(description);
         }
         else
         {
            if (std::holds_alternative<std::string>(layer.second))
            {
               return QString::fromStdString(
                  std::get<std::string>(layer.second));
            }
            else if (std::holds_alternative<awips::Phenomenon>(layer.second))
            {
               return QString::fromStdString(awips::GetPhenomenonText(
                  std::get<awips::Phenomenon>(layer.second)));
            }
         }
      }
      break;

   default:
      break;
   }

   return QVariant();
}

QVariant
LayerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (role == Qt::ItemDataRole::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
         case static_cast<int>(Column::DisplayMap1):
            return tr("1");

         case static_cast<int>(Column::DisplayMap2):
            return tr("2");

         case static_cast<int>(Column::DisplayMap3):
            return tr("3");

         case static_cast<int>(Column::DisplayMap4):
            return tr("4");

         case static_cast<int>(Column::Type):
            return tr("Type");

         case static_cast<int>(Column::Enabled):
            return tr("Enabled");

         case static_cast<int>(Column::Description):
            return tr("Description");

         default:
            break;
         }
      }
   }
   else if (role == Qt::ItemDataRole::ToolTipRole)
   {
      switch (section)
      {
      case static_cast<int>(Column::Order):
         return tr("Order");

      case static_cast<int>(Column::DisplayMap1):
         return tr("Display on Map 1");

      case static_cast<int>(Column::DisplayMap2):
         return tr("Display on Map 2");

      case static_cast<int>(Column::DisplayMap3):
         return tr("Display on Map 3");

      case static_cast<int>(Column::DisplayMap4):
         return tr("Display on Map 4");

      default:
         break;
      }
   }
   else if (role == Qt::ItemDataRole::SizeHintRole)
   {
      switch (section)
      {
      case static_cast<int>(Column::DisplayMap1):
      case static_cast<int>(Column::DisplayMap2):
      case static_cast<int>(Column::DisplayMap3):
      case static_cast<int>(Column::DisplayMap4):
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

bool LayerModel::setData(const QModelIndex& index,
                         const QVariant&    value,
                         int                role)
{
   if (!index.isValid() || index.row() < 0 ||
       static_cast<std::size_t>(index.row()) >= p->layers_.size())
   {
      return false;
   }

   const auto& layer  = p->layers_.at(index.row());
   bool        result = false;

   switch (index.column())
   {
   case static_cast<int>(Column::DisplayMap1):
   case static_cast<int>(Column::DisplayMap2):
   case static_cast<int>(Column::DisplayMap3):
   case static_cast<int>(Column::DisplayMap4):
      if (role == Qt::ItemDataRole::CheckStateRole)
      {
         // TODO
         Q_UNUSED(layer);
         Q_UNUSED(value);
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

void LayerModel::HandlePlacefileRemoved(const std::string& name)
{
   auto it = std::find_if(p->layers_.begin(),
                          p->layers_.end(),
                          [&name](const auto& layer)
                          {
                             return layer.first == LayerType::Placefile &&
                                    std::get<std::string>(layer.second) == name;
                          });

   if (it != p->layers_.end())
   {
      // Placefile exists, delete row
      const int row = std::distance(p->layers_.begin(), it);

      beginRemoveRows(QModelIndex(), row, row);
      p->layers_.erase(it);
      endRemoveRows();
   }
}

void LayerModel::HandlePlacefileRenamed(const std::string& oldName,
                                        const std::string& newName)
{
   auto it =
      std::find_if(p->layers_.begin(),
                   p->layers_.end(),
                   [&oldName](const auto& layer)
                   {
                      return layer.first == LayerType::Placefile &&
                             std::get<std::string>(layer.second) == oldName;
                   });

   if (it != p->layers_.end())
   {
      // Placefile exists, mark row as updated
      const int   row         = std::distance(p->layers_.begin(), it);
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      // Rename placefile
      it->second = newName;

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile is new, prepend row
      beginInsertRows(QModelIndex(), 0, 0);
      p->layers_.push_front({LayerType::Placefile, newName});
      endInsertRows();
   }
}

void LayerModel::HandlePlacefileUpdate(const std::string& name)
{
   auto it = std::find_if(p->layers_.begin(),
                          p->layers_.end(),
                          [&name](const auto& layer)
                          {
                             return layer.first == LayerType::Placefile &&
                                    std::get<std::string>(layer.second) == name;
                          });

   if (it != p->layers_.end())
   {
      // Placefile exists, mark row as updated
      const int   row         = std::distance(p->layers_.begin(), it);
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile is new, prepend row
      beginInsertRows(QModelIndex(), 0, 0);
      p->layers_.push_front({LayerType::Placefile, name});
      endInsertRows();
   }
}

} // namespace model
} // namespace qt
} // namespace scwx
