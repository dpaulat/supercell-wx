#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/types/map_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <set>
#include <variant>

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QIODevice>
#include <QMimeData>
#include <QStyle>
#include <QStyleOption>

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

static constexpr std::size_t kMapCount_ = 4u;

static const QString kMimeFormat {"application/x.scwx-layer-model"};

typedef std::
   variant<std::monostate, types::Layer, awips::Phenomenon, std::string>
      LayerDescription;

class LayerModel::Impl
{
public:
   struct LayerInfo
   {
      types::LayerType             type_;
      LayerDescription             description_;
      bool                         movable_;
      std::array<bool, kMapCount_> displayed_ {true, true, true, true};
   };

   typedef std::vector<LayerInfo> LayerVector;

   explicit Impl(LayerModel* self) : self_ {self}
   {
      layers_.emplace_back(
         types::LayerType::Information, types::Layer::MapOverlay, false);
      layers_.emplace_back(
         types::LayerType::Information, types::Layer::ColorTable, false);
      layers_.emplace_back(
         types::LayerType::Alert, awips::Phenomenon::Tornado, true);
      layers_.emplace_back(
         types::LayerType::Alert, awips::Phenomenon::SnowSquall, true);
      layers_.emplace_back(
         types::LayerType::Alert, awips::Phenomenon::SevereThunderstorm, true);
      layers_.emplace_back(
         types::LayerType::Alert, awips::Phenomenon::FlashFlood, true);
      layers_.emplace_back(
         types::LayerType::Alert, awips::Phenomenon::Marine, true);
      layers_.emplace_back(
         types::LayerType::Map, types::Layer::MapSymbology, false);
      layers_.emplace_back(types::LayerType::Radar, std::monostate {}, true);
      layers_.emplace_back(
         types::LayerType::Map, types::Layer::MapUnderlay, false);
   }
   ~Impl() = default;

   void AddPlacefile(const std::string& name);

   LayerModel* self_;

   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};

   LayerVector layers_ {};
};

LayerModel::LayerModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<Impl>(this))
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

   if (!index.isValid() || index.row() < 0 ||
       static_cast<std::size_t>(index.row()) >= p->layers_.size())
   {
      return flags;
   }

   const auto& layer = p->layers_.at(index.row());

   switch (index.column())
   {
   case static_cast<int>(Column::DisplayMap1):
   case static_cast<int>(Column::DisplayMap2):
   case static_cast<int>(Column::DisplayMap3):
   case static_cast<int>(Column::DisplayMap4):
      if (layer.type_ != types::LayerType::Map)
      {
         flags |=
            Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEditable;
      }
      break;

   default:
      break;
   }

   if (layer.movable_)
   {
      flags |= Qt::ItemFlag::ItemIsDragEnabled;
   }

   flags |= Qt::ItemFlag::ItemIsDropEnabled;

   return flags;
}

Qt::DropActions LayerModel::supportedDropActions() const
{
   return Qt::DropAction::MoveAction;
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

   const auto& layer = p->layers_.at(index.row());

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
      if (layer.type_ != types::LayerType::Map)
      {
         bool displayed =
            layer.displayed_[index.column() -
                             static_cast<int>(Column::DisplayMap1)];

         if (role == Qt::ItemDataRole::ToolTipRole)
         {
            return displayed ? displayedString : hiddenString;
         }
         else if (role == Qt::ItemDataRole::CheckStateRole)
         {
            return static_cast<int>(displayed ? Qt::CheckState::Checked :
                                                Qt::CheckState::Unchecked);
         }
      }
      break;

   case static_cast<int>(Column::Type):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         return QString::fromStdString(types::GetLayerTypeName(layer.type_));
      }
      break;

   case static_cast<int>(Column::Enabled):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         if (layer.type_ == types::LayerType::Placefile)
         {
            return p->placefileManager_->placefile_enabled(
                      std::get<std::string>(layer.description_)) ?
                      enabledString :
                      disabledString;
         }
      }
      break;

   case static_cast<int>(Column::Description):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         if (layer.type_ == types::LayerType::Placefile)
         {
            std::string placefileName =
               std::get<std::string>(layer.description_);
            std::string description = placefileName;
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
            if (std::holds_alternative<std::string>(layer.description_))
            {
               return QString::fromStdString(
                  std::get<std::string>(layer.description_));
            }
            else if (std::holds_alternative<types::Layer>(layer.description_))
            {
               return QString::fromStdString(types::GetLayerName(
                  std::get<types::Layer>(layer.description_)));
            }
            else if (std::holds_alternative<awips::Phenomenon>(
                        layer.description_))
            {
               return QString::fromStdString(awips::GetPhenomenonText(
                  std::get<awips::Phenomenon>(layer.description_)));
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

   auto& layer  = p->layers_.at(index.row());
   bool  result = false;

   switch (index.column())
   {
   case static_cast<int>(Column::DisplayMap1):
   case static_cast<int>(Column::DisplayMap2):
   case static_cast<int>(Column::DisplayMap3):
   case static_cast<int>(Column::DisplayMap4):
      if (role == Qt::ItemDataRole::CheckStateRole)
      {
         layer.displayed_[index.column() -
                          static_cast<int>(Column::DisplayMap1)] =
            value.toBool();
         result = true;
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

QStringList LayerModel::mimeTypes() const
{
   return {kMimeFormat};
}

QMimeData* LayerModel::mimeData(const QModelIndexList& indexes) const
{
   // Get parent QMimeData
   QMimeData* mimeData = QAbstractTableModel::mimeData(indexes);

   // Generate LayerModel data
   QByteArray    data {};
   QDataStream   stream(&data, QIODevice::WriteOnly);
   std::set<int> rows {};

   for (auto& index : indexes)
   {
      if (!rows.contains(index.row()))
      {
         rows.insert(index.row());
         stream << index.row();
      }
   }

   // Set LayerModel data in QMimeData
   mimeData->setData(kMimeFormat, data);

   return mimeData;
}

bool LayerModel::dropMimeData(const QMimeData* data,
                              Qt::DropAction /* action */,
                              int /* row */,
                              int /* column */,
                              const QModelIndex& parent)
{
   QByteArray       mimeData = data->data(kMimeFormat);
   QDataStream      stream(&mimeData, QIODevice::ReadOnly);
   std::vector<int> sourceRows {};

   // Read source rows from QMimeData
   while (!stream.atEnd())
   {
      int sourceRow;
      stream >> sourceRow;
      sourceRows.push_back(sourceRow);
   }

   // Ensure rows are in numerical order
   std::sort(sourceRows.begin(), sourceRows.end());

   if (sourceRows.back() >= p->layers_.size())
   {
      logger_->error("Cannot perform drop action, invalid source rows");
      return false;
   }

   // Nothing to insert
   if (sourceRows.empty())
   {
      return false;
   }

   // Create a copy of the layers to insert (don't insert in-place)
   std::vector<Impl::LayerInfo> newLayers {};
   for (auto& sourceRow : sourceRows)
   {
      newLayers.push_back(p->layers_.at(sourceRow));
   }

   // Insert the copied layers
   auto insertPosition = p->layers_.begin() + parent.row();
   beginInsertRows(QModelIndex(),
                   parent.row(),
                   parent.row() + static_cast<int>(sourceRows.size()) - 1);
   p->layers_.insert(insertPosition, newLayers.begin(), newLayers.end());
   endInsertRows();

   return true;
}

bool LayerModel::removeRows(int row, int count, const QModelIndex& parent)
{
   // Validate count
   if (count <= 0)
   {
      return false;
   }

   // Remove rows
   auto erasePosition = p->layers_.begin() + row;
   for (int i = 0; i < count; ++i)
   {
      if (erasePosition->movable_)
      {
         // Remove the current row if movable
         beginRemoveRows(parent, row, row);
         erasePosition = p->layers_.erase(erasePosition);
         endRemoveRows();
      }
      else
      {
         // Don't remove immovable rows
         ++erasePosition;
         ++row;
      }
   }

   return true;
}

void LayerModel::HandlePlacefileRemoved(const std::string& name)
{
   auto it =
      std::find_if(p->layers_.begin(),
                   p->layers_.end(),
                   [&name](const auto& layer)
                   {
                      return layer.type_ == types::LayerType::Placefile &&
                             std::get<std::string>(layer.description_) == name;
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
   auto it = std::find_if(
      p->layers_.begin(),
      p->layers_.end(),
      [&oldName](const auto& layer)
      {
         return layer.type_ == types::LayerType::Placefile &&
                std::get<std::string>(layer.description_) == oldName;
      });

   if (it != p->layers_.end())
   {
      // Placefile exists, mark row as updated
      const int   row         = std::distance(p->layers_.begin(), it);
      QModelIndex topLeft     = createIndex(row, kFirstColumn);
      QModelIndex bottomRight = createIndex(row, kLastColumn);

      // Rename placefile
      it->description_ = newName;

      Q_EMIT dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile doesn't exist, add row
      p->AddPlacefile(newName);
   }
}

void LayerModel::HandlePlacefileUpdate(const std::string& name)
{
   auto it =
      std::find_if(p->layers_.begin(),
                   p->layers_.end(),
                   [&name](const auto& layer)
                   {
                      return layer.type_ == types::LayerType::Placefile &&
                             std::get<std::string>(layer.description_) == name;
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
      // Placefile doesn't exist, add row
      p->AddPlacefile(name);
   }
}

void LayerModel::Impl::AddPlacefile(const std::string& name)
{
   // Insert after color table
   auto insertPosition = std::find_if(
      layers_.begin(),
      layers_.end(),
      [](const Impl::LayerInfo& layerInfo)
      {
         return std::holds_alternative<types::Layer>(layerInfo.description_) &&
                std::get<types::Layer>(layerInfo.description_) ==
                   types::Layer::ColorTable;
      });
   if (insertPosition != layers_.end())
   {
      ++insertPosition;
   }

   // Placefile is new, add row
   self_->beginInsertRows(QModelIndex(), 0, 0);
   layers_.insert(insertPosition, {types::LayerType::Placefile, name, true});
   self_->endInsertRows();
}

} // namespace model
} // namespace qt
} // namespace scwx
