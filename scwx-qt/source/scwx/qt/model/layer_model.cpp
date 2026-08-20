#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/types/map_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <set>

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QIODevice>
#include <QMimeData>
#include <QStyle>
#include <QStyleOption>
#include <boost/json.hpp>

namespace scwx::qt::model
{

static const std::string logPrefix_ = "scwx::qt::model::layer_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFirstColumn = static_cast<int>(LayerModel::Column::Order);
static constexpr int kLastColumn =
   static_cast<int>(LayerModel::Column::Description);
static constexpr int kNumColumns = kLastColumn - kFirstColumn + 1;

static const QString kMimeFormat {"application/x.scwx-layer-model"};

static const std::vector<types::LayerInfo> kDefaultLayers_ {
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::MapOverlay,
    .movable_     = false},
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::ColorTable,
    .movable_     = false},
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::RadarSite,
    .movable_     = false,
    .displayed_   = {false}},
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::Markers,
    .movable_     = true},
   {.type_        = types::LayerType::Data,
    .description_ = types::DataLayer::RadarRange,
    .movable_     = true},
   {.type_        = types::LayerType::Alert,
    .description_ = awips::Phenomenon::Tornado,
    .movable_     = true},
   {.type_        = types::LayerType::Alert,
    .description_ = awips::Phenomenon::SnowSquall,
    .movable_     = true},
   {.type_        = types::LayerType::Alert,
    .description_ = awips::Phenomenon::SevereThunderstorm,
    .movable_     = true},
   {.type_        = types::LayerType::Alert,
    .description_ = awips::Phenomenon::FlashFlood,
    .movable_     = true},
   {.type_        = types::LayerType::Alert,
    .description_ = awips::Phenomenon::Marine,
    .movable_     = true},
   {.type_        = types::LayerType::Map,
    .description_ = types::MapLayer::MapSymbology,
    .movable_     = false},
   {.type_        = types::LayerType::Data,
    .description_ = types::DataLayer::OverlayProduct,
    .movable_     = true},
   {.type_        = types::LayerType::Radar,
    .description_ = std::monostate {},
    .movable_     = true},
   {.type_        = types::LayerType::Map,
    .description_ = types::MapLayer::MapUnderlay,
    .movable_     = false},
};

static const std::vector<types::LayerInfo> kImmovableLayers_ {
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::MapOverlay,
    .movable_     = false},
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::ColorTable,
    .movable_     = false},
   {.type_        = types::LayerType::Information,
    .description_ = types::InformationLayer::RadarSite,
    .movable_     = false,
    .displayed_   = {false}},
   {.type_        = types::LayerType::Map,
    .description_ = types::MapLayer::MapSymbology,
    .movable_     = false},
   {.type_        = types::LayerType::Map,
    .description_ = types::MapLayer::MapUnderlay,
    .movable_     = false},
};

class LayerModel::Impl
{
public:
   explicit Impl(LayerModel* self) : self_ {self} {}
   ~Impl() = default;

   void AddPlacefile(const std::string& name);
   void ApplyLayerSettings(const boost::json::value& layerJson);
   void HandlePlacefileRemoved(const std::string& name);
   void HandlePlacefileRenamed(const std::string& oldName,
                               const std::string& newName);
   void HandlePlacefileUpdate(const std::string& name, Column column);
   void InitializeLayerSettings();
   void ReadLayerSettings();
   void SaveLayerSettings();
   void SynchronizePlacefileLayers();

   static void ValidateLayerSettings(types::LayerVector& layers);

   LayerModel* self_;

   std::string layerSettingsPath_ {};

   bool                     placefilesInitialized_ {false};
   std::vector<std::string> initialPlacefiles_ {};

   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};

   types::LayerVector layers_ {};

   bool fileRead_ {false};
};

LayerModel::LayerModel(QObject* parent) :
    QAbstractTableModel(parent), p(std::make_unique<Impl>(this))
{
   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefilesInitialized,
           this,
           [this]() { p->SynchronizePlacefileLayers(); });

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileEnabled,
           this,
           [this](const std::string& name, bool /* enabled */)
           { p->HandlePlacefileUpdate(name, Column::Enabled); });

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileRemoved,
           this,
           [this](const std::string& name)
           { p->HandlePlacefileRemoved(name); });

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileRenamed,
           this,
           [this](const std::string& oldName, const std::string& newName)
           { p->HandlePlacefileRenamed(oldName, newName); });

   connect(p->placefileManager_.get(),
           &manager::PlacefileManager::PlacefileUpdated,
           this,
           [this](const std::string& name)
           { p->HandlePlacefileUpdate(name, Column::Description); });

   p->InitializeLayerSettings();
   p->ReadLayerSettings();

   if (p->layers_.empty())
   {
      p->layers_.assign(kDefaultLayers_.cbegin(), kDefaultLayers_.cend());
   }
}

LayerModel::~LayerModel()
{
   // Save layer settings on shutdown
   p->SaveLayerSettings();
};

void LayerModel::Impl::InitializeLayerSettings()
{
   const std::string settingsPath {
      main::ApplicationPaths::GetLocation(
         main::ApplicationPaths::StandardLocation::Settings)
         .generic_string()};

   if (!std::filesystem::exists(settingsPath))
   {
      logger_->error("Settings path does not exist: \"{}\"", settingsPath);
   }

   layerSettingsPath_ = settingsPath + "/layers.json";
}

void LayerModel::Impl::ReadLayerSettings()
{
   logger_->info("Reading layer settings");

   boost::json::value layerJson = nullptr;

   // Determine if layer settings exists
   if (std::filesystem::exists(layerSettingsPath_))
   {
      layerJson = util::json::ReadJsonFile(layerSettingsPath_);
   }

   ApplyLayerSettings(layerJson);

   fileRead_ = true;
}

void LayerModel::ReadLayerSettings(std::istream& is)
{
   logger_->info("Reading layer settings from stream");

   const boost::json::value layerJson = scwx::util::json::ReadJsonStream(is);

   p->ApplyLayerSettings(layerJson);

   // Don't set fileRead_ when reading from a non-default stream
}

void LayerModel::Impl::ApplyLayerSettings(const boost::json::value& layerJson)
{
   types::LayerVector newLayers {};

   // If layer settings was successfully read
   if (layerJson != nullptr && layerJson.is_array())
   {
      // For each layer entry
      auto& layerArray = layerJson.as_array();
      for (auto& layerEntry : layerArray)
      {
         try
         {
            // Convert layer entry to a LayerInfo record, and add to new layers
            newLayers.emplace_back(
               boost::json::value_to<types::LayerInfo>(layerEntry));
         }
         catch (const std::exception& ex)
         {
            logger_->warn("Invalid layer entry: {}", ex.what());
         }
      }

      // Validate and correct read layers
      ValidateLayerSettings(newLayers);

      // Assign read layers
      layers_.swap(newLayers);
   }
}

void LayerModel::Impl::ValidateLayerSettings(types::LayerVector& layers)
{
   // Validate layer properties
   for (auto it = layers.begin(); it != layers.end();)
   {
      // If the layer is invalid, remove it
      if (it->type_ == types::LayerType::Unknown ||
          (std::holds_alternative<types::DataLayer>(it->description_) &&
           std::get<types::DataLayer>(it->description_) ==
              types::DataLayer::Unknown) ||
          (std::holds_alternative<types::InformationLayer>(it->description_) &&
           std::get<types::InformationLayer>(it->description_) ==
              types::InformationLayer::Unknown) ||
          (std::holds_alternative<types::MapLayer>(it->description_) &&
           std::get<types::MapLayer>(it->description_) ==
              types::MapLayer::Unknown) ||
          (std::holds_alternative<awips::Phenomenon>(it->description_) &&
           std::get<awips::Phenomenon>(it->description_) ==
              awips::Phenomenon::Unknown))
      {
         // Erase the current layer and continue
         it = layers.erase(it);
         continue;
      }

      // Ensure layers are appropriately marked movable
      it->movable_ = (it->type_ != types::LayerType::Information &&
                      it->type_ != types::LayerType::Map);

      if (types::LayerSupportsOpacity(it->type_))
      {
         it->opacity_ = types::ClampLayerOpacity(it->opacity_);
      }
      else
      {
         it->opacity_ = 1.0f;
      }

      // Continue to the next layer
      ++it;
   }

   // Validate immovable layers
   std::vector<types::LayerVector::iterator> immovableIterators {};
   for (auto& immovableLayer : kImmovableLayers_)
   {
      // Set the default displayed state for a layer that is not found
      std::array<bool, types::kMapCount_> displayed = immovableLayer.displayed_;

      // Find the immovable layer
      auto it = std::find_if(layers.begin(),
                             layers.end(),
                             [&immovableLayer](const types::LayerInfo& layer)
                             {
                                return layer.type_ == immovableLayer.type_ &&
                                       layer.description_ ==
                                          immovableLayer.description_;
                             });

      // If the immovable layer is out of order
      if (!immovableIterators.empty() && immovableIterators.back() > it)
      {
         // Save the displayed state of the immovable layer
         displayed = it->displayed_;

         // Remove the layer from the list, to re-add it later
         layers.erase(it);

         // Treat the layer as not found
         it = layers.end();
      }

      // If the immovable layer is not found
      if (it == layers.end())
      {
         // If this is the first immovable layer, insert at the beginning,
         // otherwise, insert after the previous immovable layer
         types::LayerVector::iterator insertPosition =
            immovableIterators.empty() ? layers.begin() :
                                         immovableIterators.back() + 1;
         it = layers.insert(insertPosition, immovableLayer);

         // Restore the displayed state of the immovable layer
         it->displayed_ = displayed;
      }

      // Add the immovable iterator to the list
      immovableIterators.push_back(it);
   }

   // Validate the remainder of the default layer list
   auto previousLayer = layers.end();
   for (auto defaultIt = kDefaultLayers_.rbegin();
        defaultIt != kDefaultLayers_.rend();
        ++defaultIt)
   {
      // Find the default layer in the current layer list
      auto currentIt =
         std::find_if(layers.begin(),
                      layers.end(),
                      [&defaultIt](const types::LayerInfo& layer)
                      {
                         return layer.type_ == defaultIt->type_ &&
                                layer.description_ == defaultIt->description_;
                      });

      // If the default layer was not found in the current layer list
      if (currentIt == layers.end())
      {
         // Insert before the previously found layer
         currentIt = layers.insert(previousLayer, *defaultIt);
      }

      // Store the current layer as the previous
      previousLayer = currentIt;
   }
}

void LayerModel::Impl::SaveLayerSettings()
{
   if (!fileRead_)
   {
      return;
   }
   logger_->info("Saving layer settings");

   auto layerJson = boost::json::value_from(layers_);
   util::json::WriteJsonFile(layerSettingsPath_, layerJson);
}

void LayerModel::WriteLayerSettings(std::ostream& os)
{
   auto layerJson = boost::json::value_from(p->layers_);
   util::json::WriteJsonStream(os, layerJson);
}

types::LayerInfo
LayerModel::GetLayerInfo(types::LayerType        type,
                         types::LayerDescription description) const
{
   // Find the matching layer
   auto it = std::find_if(
      p->layers_.begin(),
      p->layers_.end(),
      [&](const types::LayerInfo& layer)
      { return layer.type_ == type && layer.description_ == description; });
   if (it != p->layers_.end())
   {
      // Return the layer info
      return *it;
   }

   return {};
}

types::LayerVector LayerModel::GetLayers() const
{
   return p->layers_;
}

void LayerModel::SetLayerDisplayed(types::LayerType        type,
                                   types::LayerDescription description,
                                   bool                    displayed)
{
   // Find the matching layer
   auto it = std::find_if(
      p->layers_.begin(),
      p->layers_.end(),
      [&](const types::LayerInfo& layer)
      { return layer.type_ == type && layer.description_ == description; });

   if (it != p->layers_.end())
   {
      // Find the row
      const int   row = std::distance(p->layers_.begin(), it);
      QModelIndex topLeft =
         createIndex(row, static_cast<int>(Column::DisplayMap1));
      QModelIndex bottomRight =
         createIndex(row, static_cast<int>(Column::DisplayMap9));

      // Set the layer to displayed
      for (std::size_t i = 0; i < types::kMapCount_; ++i)
      {
         it->displayed_[i] = displayed;
      }

      // Notify observers
      Q_EMIT dataChanged(topLeft, bottomRight);
   }
}

bool LayerModel::SetLayerOpacity(types::LayerType        type,
                                 types::LayerDescription description,
                                 float                   opacity)
{
   if (!types::LayerSupportsOpacity(type))
   {
      return false;
   }

   auto it = std::find_if(
      p->layers_.begin(),
      p->layers_.end(),
      [&](const types::LayerInfo& layer)
      { return layer.type_ == type && layer.description_ == description; });

   if (it == p->layers_.end())
   {
      return false;
   }

   const float clamped = types::ClampLayerOpacity(opacity);
   if (it->opacity_ == clamped)
   {
      return false;
   }

   it->opacity_  = clamped;
   const int row = static_cast<int>(std::distance(p->layers_.begin(), it));
   const QModelIndex index =
      createIndex(row, static_cast<int>(Column::Opacity));
   Q_EMIT dataChanged(index, index);
   return true;
}

void LayerModel::ResetLayers()
{
   // Initialize a new layer vector from the default
   types::LayerVector newLayers {};
   newLayers.assign(kDefaultLayers_.cbegin(), kDefaultLayers_.cend());

   auto radarSiteIterator = std::find_if(
      newLayers.begin(),
      newLayers.end(),
      [](const types::LayerInfo& layerInfo)
      {
         return std::holds_alternative<types::InformationLayer>(
                   layerInfo.description_) &&
                std::get<types::InformationLayer>(layerInfo.description_) ==
                   types::InformationLayer::RadarSite;
      });

   // Add all existing placefile layers
   for (auto it = p->layers_.rbegin(); it != p->layers_.rend(); ++it)
   {
      if (it->type_ == types::LayerType::Placefile)
      {
         newLayers.insert(radarSiteIterator + 1, *it);
      }
   }

   // Swap the model
   beginResetModel();
   p->layers_.swap(newLayers);
   endResetModel();
}

void LayerModel::Impl::SynchronizePlacefileLayers()
{
   placefilesInitialized_ = true;

   int row = 0;
   for (auto it = layers_.begin(); it != layers_.end();)
   {
      if (it->type_ == types::LayerType::Placefile &&
          std::find(initialPlacefiles_.begin(),
                    initialPlacefiles_.end(),
                    std::get<std::string>(it->description_)) ==
             initialPlacefiles_.end())
      {
         // If the placefile layer was not loaded by the placefile manager,
         // erase it
         self_->beginRemoveRows(QModelIndex(), row, row);
         it = layers_.erase(it);
         self_->endRemoveRows();
         continue;
      }

      ++it;
      ++row;
   }

   initialPlacefiles_.clear();
}

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
   case static_cast<int>(Column::DisplayMap5):
   case static_cast<int>(Column::DisplayMap6):
   case static_cast<int>(Column::DisplayMap7):
   case static_cast<int>(Column::DisplayMap8):
   case static_cast<int>(Column::DisplayMap9):
      if (layer.type_ != types::LayerType::Map)
      {
         flags |=
            Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEditable;
      }
      break;

   case static_cast<int>(Column::Opacity):
      if (types::LayerSupportsOpacity(layer.type_))
      {
         flags |= Qt::ItemFlag::ItemIsEditable;
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

bool LayerModel::IsMovable(int row) const
{
   bool movable = false;

   if (0 <= row && static_cast<std::size_t>(row) < p->layers_.size())
   {
      movable = p->layers_.at(row).movable_;
   }

   return movable;
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
   case static_cast<int>(Column::DisplayMap5):
   case static_cast<int>(Column::DisplayMap6):
   case static_cast<int>(Column::DisplayMap7):
   case static_cast<int>(Column::DisplayMap8):
   case static_cast<int>(Column::DisplayMap9):
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

   case static_cast<int>(Column::Opacity):
      if (role == Qt::ItemDataRole::DisplayRole ||
          role == Qt::ItemDataRole::ToolTipRole)
      {
         if (!types::LayerSupportsOpacity(layer.type_))
         {
            return QObject::tr("Opaque");
         }

         return QStringLiteral("%1%").arg(
            types::LayerOpacityToPercent(layer.opacity_));
      }
      else if (role == Qt::ItemDataRole::EditRole &&
               types::LayerSupportsOpacity(layer.type_))
      {
         return types::LayerOpacityToPercent(layer.opacity_);
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
            return QString::fromStdString(
               types::GetLayerDescriptionName(layer.description_));
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

         case static_cast<int>(Column::DisplayMap5):
            return tr("5");

         case static_cast<int>(Column::DisplayMap6):
            return tr("6");

         case static_cast<int>(Column::DisplayMap7):
            return tr("7");

         case static_cast<int>(Column::DisplayMap8):
            return tr("8");

         case static_cast<int>(Column::DisplayMap9):
            return tr("9");

         case static_cast<int>(Column::Type):
            return tr("Type");

         case static_cast<int>(Column::Enabled):
            return tr("Enabled");

         case static_cast<int>(Column::Opacity):
            return tr("Opacity");

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

      case static_cast<int>(Column::DisplayMap5):
         return tr("Display on Map 5");

      case static_cast<int>(Column::DisplayMap6):
         return tr("Display on Map 6");

      case static_cast<int>(Column::DisplayMap7):
         return tr("Display on Map 7");

      case static_cast<int>(Column::DisplayMap8):
         return tr("Display on Map 8");

      case static_cast<int>(Column::DisplayMap9):
         return tr("Display on Map 9");

      case static_cast<int>(Column::Opacity):
         return tr("Layer opacity. Map style layers stay opaque.");

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
      case static_cast<int>(Column::DisplayMap5):
      case static_cast<int>(Column::DisplayMap6):
      case static_cast<int>(Column::DisplayMap7):
      case static_cast<int>(Column::DisplayMap8):
      case static_cast<int>(Column::DisplayMap9):
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
   case static_cast<int>(Column::DisplayMap5):
   case static_cast<int>(Column::DisplayMap6):
   case static_cast<int>(Column::DisplayMap7):
   case static_cast<int>(Column::DisplayMap8):
   case static_cast<int>(Column::DisplayMap9):
      if (role == Qt::ItemDataRole::CheckStateRole)
      {
         layer.displayed_[index.column() -
                          static_cast<int>(Column::DisplayMap1)] =
            value.toBool();
         result = true;
      }
      break;

   case static_cast<int>(Column::Opacity):
      if (role == Qt::ItemDataRole::EditRole &&
          types::LayerSupportsOpacity(layer.type_))
      {
         bool      ok             = false;
         const int opacityPercent = value.toInt(&ok);
         if (ok)
         {
            const float opacity =
               types::LayerOpacityFromPercent(opacityPercent);
            if (layer.opacity_ != opacity)
            {
               layer.opacity_ = opacity;
               result         = true;
            }
         }
      }
      break;

   default:
      break;
   }

   if (result)
   {
      Q_EMIT dataChanged(index, index);
      if (index.column() != static_cast<int>(Column::Opacity))
      {
         Q_EMIT LayerDisplayChanged(layer);
      }
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

   // Validate parent row
   if (parent.row() < 0 || parent.row() >= static_cast<int>(p->layers_.size()))
   {
      logger_->warn("Cannot perform drop action, invalid parent row");
      return false;
   }

   // Read source rows from QMimeData
   while (!stream.atEnd())
   {
      int sourceRow;
      stream >> sourceRow;
      sourceRows.push_back(sourceRow);
   }

   // Ensure rows are in numerical order
   std::sort(sourceRows.begin(), sourceRows.end());

   if (sourceRows.back() >= static_cast<int>(p->layers_.size()))
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
   std::vector<types::LayerInfo> newLayers {};
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

bool LayerModel::moveRows(const QModelIndex& sourceParent,
                          int                sourceRow,
                          int                count,
                          const QModelIndex& destinationParent,
                          int                destinationChild)
{
   bool moved = false;

   if (sourceParent != destinationParent || // Only accept internal moves
       count < 1 ||                         // Minimum selection size of 1
       sourceRow < 0 ||                     // Valid source row (start)
       sourceRow + count >
          static_cast<int>(p->layers_.size()) || // Valid source row (end)
       destinationChild < 0 ||                   // Valid destination row
       destinationChild > static_cast<int>(p->layers_.size()))
   {
      return false;
   }

   if (destinationChild < sourceRow)
   {
      // Move up
      auto first  = p->layers_.begin() + destinationChild;
      auto middle = p->layers_.begin() + sourceRow;
      auto last   = middle + count;

      beginMoveRows(sourceParent,
                    sourceRow,
                    sourceRow + count - 1,
                    destinationParent,
                    destinationChild);
      std::rotate(first, middle, last);
      endMoveRows();

      moved = true;
   }
   else if (sourceRow + count < destinationChild)
   {
      // Move down
      auto first  = p->layers_.begin() + sourceRow;
      auto middle = first + count;
      auto last   = p->layers_.begin() + destinationChild;

      beginMoveRows(sourceParent,
                    sourceRow,
                    sourceRow + count - 1,
                    destinationParent,
                    destinationChild);
      std::rotate(first, middle, last);
      endMoveRows();

      moved = true;
   }

   return moved;
}

void LayerModel::Impl::HandlePlacefileRemoved(const std::string& name)
{
   auto it =
      std::find_if(layers_.begin(),
                   layers_.end(),
                   [&name](const auto& layer)
                   {
                      return layer.type_ == types::LayerType::Placefile &&
                             std::get<std::string>(layer.description_) == name;
                   });

   if (it != layers_.end())
   {
      // Placefile exists, delete row
      const int row = std::distance(layers_.begin(), it);

      self_->beginRemoveRows(QModelIndex(), row, row);
      layers_.erase(it);
      self_->endRemoveRows();
   }
}

void LayerModel::Impl::HandlePlacefileRenamed(const std::string& oldName,
                                              const std::string& newName)
{
   auto it = std::find_if(
      layers_.begin(),
      layers_.end(),
      [&oldName](const auto& layer)
      {
         return layer.type_ == types::LayerType::Placefile &&
                std::get<std::string>(layer.description_) == oldName;
      });

   if (it != layers_.end())
   {
      // Placefile exists, mark row as updated
      const int   row = std::distance(layers_.begin(), it);
      QModelIndex topLeft =
         self_->createIndex(row, static_cast<int>(Column::Description));
      QModelIndex bottomRight =
         self_->createIndex(row, static_cast<int>(Column::Description));

      // Rename placefile
      it->description_ = newName;

      Q_EMIT self_->dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile doesn't exist, add row
      AddPlacefile(newName);
   }
}

void LayerModel::Impl::HandlePlacefileUpdate(const std::string& name,
                                             Column             column)
{
   if (!placefilesInitialized_)
   {
      initialPlacefiles_.push_back(name);
   }

   auto it =
      std::find_if(layers_.begin(),
                   layers_.end(),
                   [&name](const auto& layer)
                   {
                      return layer.type_ == types::LayerType::Placefile &&
                             std::get<std::string>(layer.description_) == name;
                   });

   if (it != layers_.end())
   {
      // Placefile exists, mark row as updated
      const int   row     = std::distance(layers_.begin(), it);
      QModelIndex topLeft = self_->createIndex(row, static_cast<int>(column));
      QModelIndex bottomRight =
         self_->createIndex(row, static_cast<int>(column));

      Q_EMIT self_->dataChanged(topLeft, bottomRight);
   }
   else
   {
      // Placefile doesn't exist, add row
      AddPlacefile(name);
   }
}

void LayerModel::Impl::AddPlacefile(const std::string& name)
{
   // Insert after radar site
   auto insertPosition = std::find_if(
      layers_.begin(),
      layers_.end(),
      [](const types::LayerInfo& layerInfo)
      {
         return std::holds_alternative<types::InformationLayer>(
                   layerInfo.description_) &&
                std::get<types::InformationLayer>(layerInfo.description_) ==
                   types::InformationLayer::RadarSite;
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

template<typename T, std::size_t n>
std::array<T, n> tag_invoke(boost::json::value_to_tag<std::array<T, n>>,
                            const boost::json::value& jv)
{
   std::array<T, n>   array {};
   boost::json::array jsonArray = jv.as_array();

   for (std::size_t i = 0; i < n && i < jsonArray.size(); ++i)
   {
      array[i] = jsonArray[i];
   }

   return array;
}

std::shared_ptr<LayerModel> LayerModel::Instance()
{
   static std::weak_ptr<LayerModel> layerModelReference_ {};
   static std::mutex                instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<LayerModel> layerModel = layerModelReference_.lock();

   if (layerModel == nullptr)
   {
      layerModel           = std::make_shared<LayerModel>();
      layerModelReference_ = layerModel;
   }

   return layerModel;
}

} // namespace scwx::qt::model
