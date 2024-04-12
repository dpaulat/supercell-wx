#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/map/color_table_layer.hpp>
#include <scwx/qt/map/layer_wrapper.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/map/overlay_product_layer.hpp>
#include <scwx/qt/map/placefile_layer.hpp>
#include <scwx/qt/map/radar_product_layer.hpp>
#include <scwx/qt/map/radar_range_layer.hpp>
#include <scwx/qt/map/radar_site_layer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/util/file.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/view/radar_product_view_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <set>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/random_generator.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <re2/re2.h>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QString>
#include <QTextDocument>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::map_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MapWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit MapWidgetImpl(MapWidget*                 widget,
                          std::size_t                id,
                          const QMapLibre::Settings& settings) :
       id_ {id},
       uuid_ {boost::uuids::random_generator()()},
       context_ {std::make_shared<MapContext>()},
       widget_ {widget},
       settings_(settings),
       map_(),
       layerList_ {},
       imGuiRendererInitialized_ {false},
       radarProductManager_ {nullptr},
       radarProductLayer_ {nullptr},
       alertLayer_ {std::make_shared<AlertLayer>(context_)},
       overlayLayer_ {nullptr},
       placefileLayer_ {nullptr},
       colorTableLayer_ {nullptr},
       autoRefreshEnabled_ {true},
       autoUpdateEnabled_ {true},
       selectedLevel2Product_ {common::Level2Product::Unknown},
       currentStyleIndex_ {0},
       currentStyle_ {nullptr},
       frameDraws_(0),
       prevLatitude_ {0.0},
       prevLongitude_ {0.0},
       prevZoom_ {0.0},
       prevBearing_ {0.0},
       prevPitch_ {0.0}
   {
      // Create views
      auto overlayProductView = std::make_shared<view::OverlayProductView>();
      overlayProductView->SetAutoRefresh(autoRefreshEnabled_);
      overlayProductView->SetAutoUpdate(autoUpdateEnabled_);

      auto& generalSettings = settings::GeneralSettings::Instance();

      // Initialize context
      context_->set_map_provider(
         GetMapProvider(generalSettings.map_provider().GetValue()));
      context_->set_overlay_product_view(overlayProductView);

      SetRadarSite(generalSettings.default_radar_site().GetValue());

      // Create ImGui Context
      static size_t currentMapId_ {0u};
      imGuiContextName_ = fmt::format("Map {}", ++currentMapId_);
      imGuiContext_ =
         model::ImGuiContextModel::Instance().CreateContext(imGuiContextName_);

      // Initialize ImGui Qt backend
      ImGui_ImplQt_Init();

      ConnectSignals();
   }

   ~MapWidgetImpl()
   {
      // Set ImGui Context
      ImGui::SetCurrentContext(imGuiContext_);

      // Shutdown ImGui Context
      if (imGuiRendererInitialized_)
      {
         ImGui_ImplOpenGL3_Shutdown();
      }
      ImGui_ImplQt_Shutdown();

      // Destroy ImGui Context
      model::ImGuiContextModel::Instance().DestroyContext(imGuiContextName_);

      threadPool_.join();
   }

   void AddLayer(types::LayerType        type,
                 types::LayerDescription description,
                 const std::string&      before = {});
   void AddLayer(const std::string&            id,
                 std::shared_ptr<GenericLayer> layer,
                 const std::string&            before = {});
   void AddLayers();
   void AddPlacefileLayer(const std::string& placefileName,
                          const std::string& before);
   void ConnectMapSignals();
   void ConnectSignals();
   void HandleHotkey(types::Hotkey hotkey, bool isAutoRepeat);
   void ImGuiCheckFonts();
   void InitializeNewRadarProductView(const std::string& colorPalette);
   void RadarProductManagerConnect();
   void RadarProductManagerDisconnect();
   void RadarProductViewConnect();
   void RadarProductViewDisconnect();
   void RunMousePicking();
   void SetRadarSite(const std::string& radarSite);
   void UpdateLoadedStyle();
   bool UpdateStoredMapParameters();

   std::string FindMapSymbologyLayer();

   common::Level2Product
   GetLevel2ProductOrDefault(const std::string& productName) const;

   static std::string GetPlacefileLayerName(const std::string& placefileName);

   boost::asio::thread_pool threadPool_ {1u};

   std::size_t        id_;
   boost::uuids::uuid uuid_;

   std::shared_ptr<MapContext> context_;

   MapWidget*                      widget_;
   QMapLibre::Settings             settings_;
   std::shared_ptr<QMapLibre::Map> map_;
   std::list<std::string>          layerList_;

   std::vector<std::shared_ptr<GenericLayer>> genericLayers_ {};

   QStringList        styleLayers_;
   types::LayerVector customLayers_;

   ImGuiContext* imGuiContext_;
   std::string   imGuiContextName_;
   bool          imGuiRendererInitialized_;
   std::uint64_t imGuiFontsBuildCount_ {};

   std::shared_ptr<model::LayerModel> layerModel_ {
      model::LayerModel::Instance()};

   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};
   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};
   std::shared_ptr<manager::RadarProductManager> radarProductManager_;

   std::shared_ptr<RadarProductLayer>   radarProductLayer_;
   std::shared_ptr<AlertLayer>          alertLayer_;
   std::shared_ptr<OverlayLayer>        overlayLayer_;
   std::shared_ptr<OverlayProductLayer> overlayProductLayer_ {nullptr};
   std::shared_ptr<PlacefileLayer>      placefileLayer_;
   std::shared_ptr<ColorTableLayer>     colorTableLayer_;
   std::shared_ptr<RadarSiteLayer>      radarSiteLayer_ {nullptr};

   std::list<std::shared_ptr<PlacefileLayer>> placefileLayers_ {};

   bool autoRefreshEnabled_;
   bool autoUpdateEnabled_;

   common::Level2Product selectedLevel2Product_;

   bool            hasMouse_ {false};
   bool            lastItemPicked_ {false};
   QPointF         lastPos_ {};
   QPointF         lastGlobalPos_ {};
   std::size_t     currentStyleIndex_;
   const MapStyle* currentStyle_;
   std::string     initialStyleName_ {};

   std::shared_ptr<types::EventHandler> pickedEventHandler_ {nullptr};

   uint64_t frameDraws_;

   double prevLatitude_;
   double prevLongitude_;
   double prevZoom_;
   double prevBearing_;
   double prevPitch_;

   types::Hotkey                         prevHotkey_ = types::Hotkey::Unknown;
   std::chrono::system_clock::time_point prevHotkeyTime_ {};
public slots:
   void Update();
};

MapWidget::MapWidget(std::size_t id, const QMapLibre::Settings& settings) :
    p(std::make_unique<MapWidgetImpl>(this, id, settings))
{
   if (settings::GeneralSettings::Instance().anti_aliasing_enabled().GetValue())
   {
      QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
      surfaceFormat.setSamples(4);
      setFormat(surfaceFormat);
   }

   setFocusPolicy(Qt::StrongFocus);

   ImGui_ImplQt_RegisterWidget(this);
}

MapWidget::~MapWidget()
{
   // Make sure we have a valid context so we can delete the QMapLibre.
   makeCurrent();
}

void MapWidgetImpl::ConnectMapSignals()
{
   connect(map_.get(),
           &QMapLibre::Map::needsRendering,
           this,
           &MapWidgetImpl::Update);
   connect(map_.get(),
           &QMapLibre::Map::copyrightsChanged,
           this,
           [this](const QString& copyrightsHtml)
           {
              QTextDocument document {};
              document.setHtml(copyrightsHtml);

              // HTML cannot currently be included in ImGui windows. Where links
              // can't be included, remove "Improve this map".
              std::string copyrights {document.toPlainText().toStdString()};
              boost::erase_all(copyrights, "Improve this map");
              boost::trim_right(copyrights);

              context_->set_map_copyrights(copyrights);
           });
}

void MapWidgetImpl::ConnectSignals()
{
   connect(placefileManager_.get(),
           &manager::PlacefileManager::PlacefileUpdated,
           widget_,
           [this]() { widget_->update(); });

   // When the layer model changes, update the layers
   connect(layerModel_.get(),
           &QAbstractItemModel::dataChanged,
           widget_,
           [this](const QModelIndex& topLeft,
                  const QModelIndex& bottomRight,
                  const QList<int>& /* roles */)
           {
              static const int enabledColumn =
                 static_cast<int>(model::LayerModel::Column::Enabled);
              const int displayColumn =
                 static_cast<int>(model::LayerModel::Column::DisplayMap1) +
                 static_cast<int>(id_);

              // Update layers if the displayed or enabled state of the layer
              // has changed
              if ((topLeft.column() <= displayColumn &&
                   displayColumn <= bottomRight.column()) ||
                  (topLeft.column() <= enabledColumn &&
                   enabledColumn <= bottomRight.column()))
              {
                 AddLayers();
              }
           });
   connect(layerModel_.get(),
           &QAbstractItemModel::modelReset,
           widget_,
           [this]() { AddLayers(); });
   connect(layerModel_.get(),
           &QAbstractItemModel::rowsInserted,
           widget_,
           [this](const QModelIndex& /* parent */, //
                  int /* first */,
                  int /* last */) { AddLayers(); });
   connect(layerModel_.get(),
           &QAbstractItemModel::rowsMoved,
           widget_,
           [this](const QModelIndex& /* sourceParent */,
                  int /* sourceStart */,
                  int /* sourceEnd */,
                  const QModelIndex& /* destinationParent */,
                  int /* destinationRow */) { AddLayers(); });
   connect(layerModel_.get(),
           &QAbstractItemModel::rowsRemoved,
           widget_,
           [this](const QModelIndex& /* parent */, //
                  int /* first */,
                  int /* last */) { AddLayers(); });

   connect(hotkeyManager_.get(),
           &manager::HotkeyManager::HotkeyPressed,
           this,
           &MapWidgetImpl::HandleHotkey);
}

void MapWidgetImpl::HandleHotkey(types::Hotkey hotkey, bool isAutoRepeat)
{
   static constexpr float  kMapPanFactor    = 0.2f;
   static constexpr float  kMapRotateFactor = 0.2f;
   static constexpr double kMapScaleFactor  = 1000.0;

   using namespace std::chrono_literals;

   std::chrono::system_clock::time_point hotkeyTime =
      std::chrono::system_clock::now();
   std::chrono::milliseconds hotkeyElapsed =
      isAutoRepeat ? std::chrono::duration_cast<std::chrono::milliseconds>(
                        hotkeyTime - prevHotkeyTime_) :
                     100ms;

   switch (hotkey)
   {
   case types::Hotkey::ChangeMapStyle:
      widget_->changeStyle();
      break;

   case types::Hotkey::CopyCursorCoordinates:
   {
      QClipboard* clipboard  = QGuiApplication::clipboard();
      auto        coordinate = map_->coordinateForPixel(lastPos_);
      std::string text =
         fmt::format("{}, {}", coordinate.first, coordinate.second);
      clipboard->setText(QString::fromStdString(text));
      break;
   }

   case types::Hotkey::CopyMapCoordinates:
   {
      QClipboard* clipboard  = QGuiApplication::clipboard();
      auto        coordinate = map_->coordinate();
      std::string text =
         fmt::format("{}, {}", coordinate.first, coordinate.second);
      clipboard->setText(QString::fromStdString(text));
      break;
   }

   case types::Hotkey::MapPanUp:
   {
      QPointF delta {0.0f, kMapPanFactor * hotkeyElapsed.count()};
      map_->moveBy(delta);
      break;
   }

   case types::Hotkey::MapPanDown:
   {
      QPointF delta {0.0f, -kMapPanFactor * hotkeyElapsed.count()};
      map_->moveBy(delta);
      break;
   }

   case types::Hotkey::MapPanLeft:
   {
      QPointF delta {kMapPanFactor * hotkeyElapsed.count(), 0.0f};
      map_->moveBy(delta);
      break;
   }

   case types::Hotkey::MapPanRight:
   {
      QPointF delta {-kMapPanFactor * hotkeyElapsed.count(), 0.0f};
      map_->moveBy(delta);
      break;
   }

   case types::Hotkey::MapRotateClockwise:
   {
      QPointF delta {-kMapRotateFactor * hotkeyElapsed.count(), 0.0f};
      map_->rotateBy({}, delta);
      break;
   }

   case types::Hotkey::MapRotateCounterclockwise:
   {
      QPointF delta {kMapRotateFactor * hotkeyElapsed.count(), 0.0f};
      map_->rotateBy({}, delta);
      break;
   }

   case types::Hotkey::MapZoomIn:
   {
      auto    widgetSize = widget_->size();
      QPointF center = {widgetSize.width() * 0.5f, widgetSize.height() * 0.5f};
      double  scale  = std::pow(2.0, hotkeyElapsed.count() / kMapScaleFactor);
      map_->scaleBy(scale, center);
      break;
   }

   case types::Hotkey::MapZoomOut:
   {
      auto    widgetSize = widget_->size();
      QPointF center = {widgetSize.width() * 0.5f, widgetSize.height() * 0.5f};
      double  scale =
         1.0 / std::pow(2.0, hotkeyElapsed.count() / kMapScaleFactor);
      map_->scaleBy(scale, center);
      break;
   }

   default:
      break;
   }

   prevHotkey_     = hotkey;
   prevHotkeyTime_ = hotkeyTime;
}

common::Level3ProductCategoryMap MapWidget::GetAvailableLevel3Categories()
{
   if (p->radarProductManager_ != nullptr)
   {
      return p->radarProductManager_->GetAvailableLevel3Categories();
   }
   else
   {
      return {};
   }
}

float MapWidget::GetElevation() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->elevation();
   }
   else
   {
      return 0.0f;
   }
}

std::vector<float> MapWidget::GetElevationCuts() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->GetElevationCuts();
   }
   else
   {
      return {};
   }
}

common::Level2Product
MapWidgetImpl::GetLevel2ProductOrDefault(const std::string& productName) const
{
   common::Level2Product level2Product = common::GetLevel2Product(productName);

   if (level2Product == common::Level2Product::Unknown)
   {
      auto radarProductView = context_->radar_product_view();

      if (radarProductView != nullptr)
      {
         level2Product =
            common::GetLevel2Product(radarProductView->GetRadarProductName());
      }
   }

   if (level2Product == common::Level2Product::Unknown)
   {
      if (selectedLevel2Product_ != common::Level2Product::Unknown)
      {
         level2Product = selectedLevel2Product_;
      }
      else
      {
         level2Product = common::Level2Product::Reflectivity;
      }
   }

   return level2Product;
}

std::vector<std::string> MapWidget::GetLevel3Products()
{
   if (p->radarProductManager_ != nullptr)
   {
      return p->radarProductManager_->GetLevel3Products();
   }
   else
   {
      return {};
   }
}

std::string MapWidget::GetMapStyle() const
{
   if (p->currentStyle_ != nullptr)
   {
      return p->currentStyle_->name_;
   }
   else
   {
      return "?";
   }
}

common::RadarProductGroup MapWidget::GetRadarProductGroup() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->GetRadarProductGroup();
   }
   else
   {
      return common::RadarProductGroup::Unknown;
   }
}

std::string MapWidget::GetRadarProductName() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->GetRadarProductName();
   }
   else
   {
      return "?";
   }
}

std::shared_ptr<config::RadarSite> MapWidget::GetRadarSite() const
{
   std::shared_ptr<config::RadarSite> radarSite = nullptr;

   if (p->radarProductManager_ != nullptr)
   {
      radarSite = p->radarProductManager_->radar_site();
   }

   return radarSite;
}

std::chrono::system_clock::time_point MapWidget::GetSelectedTime() const
{
   auto radarProductView = p->context_->radar_product_view();
   std::chrono::system_clock::time_point time;

   // If there is an active radar product view
   if (radarProductView != nullptr)
   {
      // Select the time associated with the active radar product
      time = radarProductView->selected_time();
   }

   return time;
}

std::uint16_t MapWidget::GetVcp() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->vcp();
   }
   else
   {
      return 0u;
   }
}

void MapWidget::SelectElevation(float elevation)
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      radarProductView->SelectElevation(elevation);
      radarProductView->Update();
   }
}

void MapWidget::SelectRadarProduct(common::RadarProductGroup group,
                                   const std::string&        product,
                                   std::int16_t              productCode,
                                   std::chrono::system_clock::time_point time,
                                   bool                                  update)
{
   bool radarProductViewCreated = false;

   auto radarProductView = p->context_->radar_product_view();

   std::string productName {product};

   // Validate level 2 product, set to default if invalid
   if (group == common::RadarProductGroup::Level2)
   {
      common::Level2Product level2Product =
         p->GetLevel2ProductOrDefault(productName);
      productName               = common::GetLevel2Name(level2Product);
      p->selectedLevel2Product_ = level2Product;
   }

   if (group == common::RadarProductGroup::Level3 && productCode == 0)
   {
      productCode = common::GetLevel3ProductCodeByAwipsId(productName);
   }

   if (radarProductView == nullptr ||
       radarProductView->GetRadarProductGroup() != group ||
       (radarProductView->GetRadarProductGroup() ==
           common::RadarProductGroup::Level2 &&
        radarProductView->GetRadarProductName() != productName) ||
       p->context_->radar_product_code() != productCode)
   {
      p->RadarProductViewDisconnect();

      radarProductView = view::RadarProductViewFactory::Create(
         group, productName, productCode, p->radarProductManager_);
      p->context_->set_radar_product_view(radarProductView);

      p->RadarProductViewConnect();

      radarProductViewCreated = true;
   }
   else
   {
      radarProductView->SelectProduct(productName);
   }

   p->context_->set_radar_product_group(group);
   p->context_->set_radar_product(productName);
   p->context_->set_radar_product_code(productCode);

   if (radarProductView != nullptr)
   {
      // Select the time associated with the request
      radarProductView->SelectTime(time);

      if (radarProductViewCreated)
      {
         const std::string palette =
            (group == common::RadarProductGroup::Level2) ?
               common::GetLevel2Palette(common::GetLevel2Product(productName)) :
               common::GetLevel3Palette(productCode);
         p->InitializeNewRadarProductView(palette);
      }
      else if (update)
      {
         radarProductView->Update();
      }
   }

   if (p->autoRefreshEnabled_)
   {
      p->radarProductManager_->EnableRefresh(
         group, productName, true, p->uuid_);
   }
}

void MapWidget::SelectRadarProduct(
   std::shared_ptr<types::RadarProductRecord> record)
{
   const std::string                     radarId = record->radar_id();
   common::RadarProductGroup             group = record->radar_product_group();
   const std::string                     product     = record->radar_product();
   std::chrono::system_clock::time_point time        = record->time();
   int16_t                               productCode = record->product_code();

   logger_->debug("SelectRadarProduct: {}, {}, {}, {}",
                  radarId,
                  common::GetRadarProductGroupName(group),
                  product,
                  scwx::util::TimeString(time));

   p->SetRadarSite(radarId);

   SelectRadarProduct(group, product, productCode, time);
}

void MapWidget::SelectRadarSite(const std::string& id, bool updateCoordinates)
{
   logger_->debug("Selecting radar site: {}", id);

   std::shared_ptr<config::RadarSite> radarSite = config::RadarSite::Get(id);

   SelectRadarSite(radarSite, updateCoordinates);
}

void MapWidget::SelectRadarSite(std::shared_ptr<config::RadarSite> radarSite,
                                bool updateCoordinates)
{
   // Verify radar site is valid and has changed
   if (radarSite != nullptr &&
       (p->radarProductManager_ == nullptr ||
        radarSite->id() != p->radarProductManager_->radar_site()->id()))
   {
      auto radarProductView = p->context_->radar_product_view();

      if (updateCoordinates)
      {
         p->map_->setCoordinate(
            {radarSite->latitude(), radarSite->longitude()});
      }
      p->SetRadarSite(radarSite->id());
      p->Update();

      // Select products from new site
      if (radarProductView != nullptr)
      {
         radarProductView->set_radar_product_manager(p->radarProductManager_);
         SelectRadarProduct(radarProductView->GetRadarProductGroup(),
                            radarProductView->GetRadarProductName(),
                            0,
                            radarProductView->selected_time(),
                            false);
      }

      p->AddLayers();

      // TODO: Disable refresh from old site

      Q_EMIT RadarSiteUpdated(radarSite);
   }
}

void MapWidget::SelectTime(std::chrono::system_clock::time_point time)
{
   auto radarProductView = p->context_->radar_product_view();

   // Update other views
   p->context_->overlay_product_view()->SelectTime(time);

   // If there is an active radar product view
   if (radarProductView != nullptr)
   {
      // Select the time associated with the active radar product
      radarProductView->SelectTime(time);

      // Trigger an update of the radar product view
      radarProductView->Update();
   }
}

void MapWidget::SetActive(bool isActive)
{
   p->context_->settings().isActive_ = isActive;
   update();
}

void MapWidget::SetAutoRefresh(bool enabled)
{
   if (p->autoRefreshEnabled_ != enabled)
   {
      p->autoRefreshEnabled_ = enabled;

      auto radarProductView = p->context_->radar_product_view();

      if (p->autoRefreshEnabled_ && radarProductView != nullptr)
      {
         p->radarProductManager_->EnableRefresh(
            radarProductView->GetRadarProductGroup(),
            radarProductView->GetRadarProductName(),
            true,
            p->uuid_);
      }

      p->context_->overlay_product_view()->SetAutoRefresh(enabled);
   }
}

void MapWidget::SetAutoUpdate(bool enabled)
{
   p->autoUpdateEnabled_ = enabled;

   p->context_->overlay_product_view()->SetAutoUpdate(enabled);
}

void MapWidget::SetMapLocation(double latitude,
                               double longitude,
                               bool   updateRadarSite)
{
   if (p->map_ != nullptr &&
       (p->prevLatitude_ != latitude || p->prevLongitude_ != longitude))
   {
      // Update the map location
      p->map_->setCoordinate({latitude, longitude});

      // If the radar site should be updated based on the new location
      if (updateRadarSite)
      {
         // Find the nearest WSR-88D radar
         std::shared_ptr<config::RadarSite> nearestRadarSite =
            config::RadarSite::FindNearest(latitude, longitude, "wsr88d");

         // If found, select it
         if (nearestRadarSite != nullptr)
         {
            SelectRadarSite(nearestRadarSite->id(), false);
         }
      }
   }
}

void MapWidget::SetMapParameters(
   double latitude, double longitude, double zoom, double bearing, double pitch)
{
   if (p->map_ != nullptr &&
       (p->prevLatitude_ != latitude || p->prevLongitude_ != longitude ||
        p->prevZoom_ != zoom || p->prevBearing_ != bearing ||
        p->prevPitch_ != pitch))
   {
      p->map_->setCoordinateZoom({latitude, longitude}, zoom);
      p->map_->setBearing(bearing);
      p->map_->setPitch(pitch);
   }
}

void MapWidget::SetInitialMapStyle(const std::string& styleName)
{
   p->initialStyleName_ = styleName;
}

void MapWidget::SetMapStyle(const std::string& styleName)
{
   const auto  mapProvider     = p->context_->map_provider();
   const auto& mapProviderInfo = GetMapProviderInfo(mapProvider);
   auto&       styles          = mapProviderInfo.mapStyles_;

   for (size_t i = 0u; i < styles.size(); ++i)
   {
      if (styles[i].name_ == styleName)
      {
         p->currentStyleIndex_ = i;
         p->currentStyle_      = &styles[i];

         logger_->debug("Updating style: {}", styles[i].name_);

         util::maplibre::SetMapStyleUrl(p->context_, styles[i].url_);

         if (++p->currentStyleIndex_ == styles.size())
         {
            p->currentStyleIndex_ = 0;
         }

         break;
      }
   }
}

qreal MapWidget::pixelRatio()
{
   return devicePixelRatioF();
}

void MapWidget::changeStyle()
{
   const auto  mapProvider     = p->context_->map_provider();
   const auto& mapProviderInfo = GetMapProviderInfo(mapProvider);
   auto&       styles          = mapProviderInfo.mapStyles_;

   p->currentStyle_ = &styles[p->currentStyleIndex_];

   logger_->debug("Updating style: {}", styles[p->currentStyleIndex_].name_);

   util::maplibre::SetMapStyleUrl(p->context_,
                                  styles[p->currentStyleIndex_].url_);

   if (++p->currentStyleIndex_ == styles.size())
   {
      p->currentStyleIndex_ = 0;
   }

   Q_EMIT MapStyleChanged(p->currentStyle_->name_);
}

void MapWidget::DumpLayerList() const
{
   logger_->info("Layers: {}", p->map_->layerIds().join(", ").toStdString());
}

std::string MapWidgetImpl::FindMapSymbologyLayer()
{
   std::string before = "ferry";

   for (const QString& qlayer : styleLayers_)
   {
      const std::string layer = qlayer.toStdString();

      // Draw below layers defined in map style
      auto it = std::find_if(currentStyle_->drawBelow_.cbegin(),
                             currentStyle_->drawBelow_.cend(),
                             [&layer](const std::string& styleLayer) -> bool
                             {
                                // Perform case-insensitive matching
                                RE2 re {"(?i)" + styleLayer};
                                return RE2::FullMatch(layer, re);
                             });

      if (it != currentStyle_->drawBelow_.cend())
      {
         before = layer;
         break;
      }
   }

   return before;
}

void MapWidgetImpl::AddLayers()
{
   if (styleLayers_.isEmpty())
   {
      // Skip if the map has not yet been initialized
      return;
   }

   logger_->debug("Add Layers");

   // Clear custom layers
   for (const std::string& id : layerList_)
   {
      map_->removeLayer(id.c_str());
   }
   layerList_.clear();
   genericLayers_.clear();
   placefileLayers_.clear();

   // Update custom layer list from model
   customLayers_ = model::LayerModel::Instance()->GetLayers();

   // Start by drawing layers before any style-defined layers
   std::string before = styleLayers_.front().toStdString();

   // Loop through each custom layer in reverse order
   for (auto it = customLayers_.crbegin(); it != customLayers_.crend(); ++it)
   {
      if (it->type_ == types::LayerType::Map)
      {
         // Style-defined map layers
         switch (std::get<types::MapLayer>(it->description_))
         {
         // Subsequent layers are drawn underneath the map symbology layer
         case types::MapLayer::MapUnderlay:
            before = FindMapSymbologyLayer();
            break;

         // Subsequent layers are drawn after all style-defined layers
         case types::MapLayer::MapSymbology:
            before = "";
            break;

         default:
            break;
         }
      }
      else if (it->displayed_[id_])
      {
         // If the layer is displayed for the current map, add it
         AddLayer(it->type_, it->description_, before);
      }
   }
}

void MapWidgetImpl::AddLayer(types::LayerType        type,
                             types::LayerDescription description,
                             const std::string&      before)
{
   std::string layerName = types::GetLayerName(type, description);

   auto radarProductView = context_->radar_product_view();

   if (type == types::LayerType::Radar)
   {
      // If there is a radar product view, create the radar product layer
      if (radarProductView != nullptr)
      {
         radarProductLayer_ = std::make_shared<RadarProductLayer>(context_);
         AddLayer(layerName, radarProductLayer_, before);
      }
   }
   else if (type == types::LayerType::Alert)
   {
      // Add the alert layer for the phenomenon
      auto newLayers = alertLayer_->AddLayers(
         std::get<awips::Phenomenon>(description), before);
      layerList_.insert(layerList_.end(), newLayers.cbegin(), newLayers.cend());
   }
   else if (type == types::LayerType::Placefile)
   {
      // If the placefile is enabled, add the placefile layer
      std::string placefileName = std::get<std::string>(description);
      if (placefileManager_->placefile_enabled(placefileName))
      {
         AddPlacefileLayer(placefileName, before);
      }
   }
   else if (type == types::LayerType::Information)
   {
      switch (std::get<types::InformationLayer>(description))
      {
      // Create the map overlay layer
      case types::InformationLayer::MapOverlay:
         overlayLayer_ = std::make_shared<OverlayLayer>(context_);
         AddLayer(layerName, overlayLayer_, before);
         break;

      // If there is a radar product view, create the color table layer
      case types::InformationLayer::ColorTable:
         if (radarProductView != nullptr)
         {
            colorTableLayer_ = std::make_shared<ColorTableLayer>(context_);
            AddLayer(layerName, colorTableLayer_, before);
         }
         break;

      // Create the radar site layer
      case types::InformationLayer::RadarSite:
         radarSiteLayer_ = std::make_shared<RadarSiteLayer>(context_);
         AddLayer(layerName, radarSiteLayer_, before);
         connect(radarSiteLayer_.get(),
                 &RadarSiteLayer::RadarSiteSelected,
                 widget_,
                 &MapWidget::RadarSiteRequested);
         break;

      default:
         break;
      }
   }
   else if (type == types::LayerType::Data)
   {
      switch (std::get<types::DataLayer>(description))
      {
      // If there is a radar product view, create the overlay product layer
      case types::DataLayer::OverlayProduct:
         if (radarProductView != nullptr)
         {
            overlayProductLayer_ =
               std::make_shared<OverlayProductLayer>(context_);
            AddLayer(layerName, overlayProductLayer_, before);
         }
         break;

      // If there is a radar product view, create the radar range layer
      case types::DataLayer::RadarRange:
         if (radarProductView != nullptr)
         {
            std::shared_ptr<config::RadarSite> radarSite =
               radarProductManager_->radar_site();
            RadarRangeLayer::Add(
               map_,
               radarProductView->range(),
               {radarSite->latitude(), radarSite->longitude()},
               QString::fromStdString(before));
            layerList_.push_back(types::GetLayerName(type, description));
         }
         break;

      default:
         break;
      }
   }
}

void MapWidgetImpl::AddPlacefileLayer(const std::string& placefileName,
                                      const std::string& before)
{
   std::shared_ptr<PlacefileLayer> placefileLayer =
      std::make_shared<PlacefileLayer>(context_, placefileName);
   placefileLayers_.push_back(placefileLayer);
   AddLayer(GetPlacefileLayerName(placefileName), placefileLayer, before);

   // When the layer updates, trigger a map widget update
   connect(placefileLayer.get(),
           &PlacefileLayer::DataReloaded,
           widget_,
           [this]() { widget_->update(); });
}

std::string
MapWidgetImpl::GetPlacefileLayerName(const std::string& placefileName)
{
   return types::GetLayerName(types::LayerType::Placefile, placefileName);
}

void MapWidgetImpl::AddLayer(const std::string&            id,
                             std::shared_ptr<GenericLayer> layer,
                             const std::string&            before)
{
   // QMapLibre::addCustomLayer will take ownership of the std::unique_ptr
   std::unique_ptr<QMapLibre::CustomLayerHostInterface> pHost =
      std::make_unique<LayerWrapper>(layer);

   try
   {
      map_->addCustomLayer(id.c_str(), std::move(pHost), before.c_str());

      layerList_.push_back(id);
      genericLayers_.push_back(layer);

      connect(layer.get(),
              &GenericLayer::NeedsRendering,
              widget_,
              [this]() { widget_->update(); });
   }
   catch (const std::exception&)
   {
      // When dragging and dropping, a temporary duplicate layer exists
   }
}

bool MapWidget::event(QEvent* e)
{
   if (p->pickedEventHandler_ != nullptr &&
       p->pickedEventHandler_->event_ != nullptr)
   {
      p->pickedEventHandler_->event_(e);
   }

   return QOpenGLWidget::event(e);
}

void MapWidget::enterEvent(QEnterEvent* /* ev */)
{
   p->hasMouse_ = true;
}

void MapWidget::leaveEvent(QEvent* /* ev */)
{
   p->hasMouse_ = false;
}

void MapWidget::keyPressEvent(QKeyEvent* ev)
{
   p->hotkeyManager_->HandleKeyPress(ev);

   ev->accept();
}

void MapWidget::keyReleaseEvent(QKeyEvent* ev)
{
   p->hotkeyManager_->HandleKeyRelease(ev);

   ev->accept();
}

void MapWidget::mousePressEvent(QMouseEvent* ev)
{
   p->lastPos_       = ev->position();
   p->lastGlobalPos_ = ev->globalPosition();

   if (ev->type() == QEvent::MouseButtonPress)
   {
      if (ev->buttons() == (Qt::LeftButton | Qt::RightButton))
      {
         changeStyle();
      }
   }

   if (ev->type() == QEvent::MouseButtonDblClick)
   {
      if (ev->buttons() == Qt::LeftButton)
      {
         p->map_->scaleBy(2.0, p->lastPos_);
      }
      else if (ev->buttons() == Qt::RightButton)
      {
         p->map_->scaleBy(0.5, p->lastPos_);
      }
   }

   ev->accept();
}

void MapWidget::mouseMoveEvent(QMouseEvent* ev)
{
   QPointF delta = ev->position() - p->lastPos_;

   if (!delta.isNull())
   {
      if (ev->buttons() == Qt::LeftButton)
      {
         p->map_->moveBy(delta);
      }
      else if (ev->buttons() == Qt::RightButton)
      {
         p->map_->rotateBy(p->lastPos_, ev->position());
      }
   }

   p->lastPos_       = ev->position();
   p->lastGlobalPos_ = ev->globalPosition();
   ev->accept();
}

void MapWidget::wheelEvent(QWheelEvent* ev)
{
   if (ev->angleDelta().y() == 0)
   {
      return;
   }

   float factor = ev->angleDelta().y() / 1200.;
   if (ev->angleDelta().y() < 0)
   {
      factor = factor > -1 ? factor : 1 / factor;
   }

   p->map_->scaleBy(1 + factor, ev->position());

   ev->accept();
}

void MapWidget::initializeGL()
{
   logger_->debug("initializeGL()");

   makeCurrent();
   p->context_->gl().initializeOpenGLFunctions();

   // Lock ImGui font atlas prior to new ImGui frame
   std::shared_lock imguiFontAtlasLock {
      manager::FontManager::Instance().imgui_font_atlas_mutex()};

   // Initialize ImGui OpenGL3 backend
   ImGui::SetCurrentContext(p->imGuiContext_);
   ImGui_ImplQt_RegisterWidget(this);
   ImGui_ImplOpenGL3_Init();
   p->imGuiFontsBuildCount_ =
      manager::FontManager::Instance().imgui_fonts_build_count();
   p->imGuiRendererInitialized_ = true;

   p->map_.reset(
      new QMapLibre::Map(nullptr, p->settings_, size(), pixelRatio()));
   p->context_->set_map(p->map_);
   p->ConnectMapSignals();

   // Set default location to radar site
   std::shared_ptr<config::RadarSite> radarSite =
      p->radarProductManager_->radar_site();
   p->map_->setCoordinateZoom({radarSite->latitude(), radarSite->longitude()},
                              7);
   p->UpdateStoredMapParameters();
   Q_EMIT MapParametersChanged(p->prevLatitude_,
                               p->prevLongitude_,
                               p->prevZoom_,
                               p->prevBearing_,
                               p->prevPitch_);

   // Update style
   if (p->initialStyleName_.empty())
   {
      changeStyle();
   }
   else
   {
      SetMapStyle(p->initialStyleName_);
   }

   connect(
      p->map_.get(), &QMapLibre::Map::mapChanged, this, &MapWidget::mapChanged);
}

void MapWidget::paintGL()
{
   auto defaultFont = manager::FontManager::Instance().GetImGuiFont(
      types::FontCategory::Default);

   p->frameDraws_++;

   // Setup ImGui Frame
   ImGui::SetCurrentContext(p->imGuiContext_);

   // Lock ImGui font atlas prior to new ImGui frame
   std::shared_lock imguiFontAtlasLock {
      manager::FontManager::Instance().imgui_font_atlas_mutex()};

   // Start ImGui Frame
   ImGui_ImplQt_NewFrame(this);
   ImGui_ImplOpenGL3_NewFrame();
   p->ImGuiCheckFonts();
   ImGui::NewFrame();

   // Set default font
   ImGui::PushFont(defaultFont->font());

   // Update pixel ratio
   p->context_->set_pixel_ratio(pixelRatio());

   // Render QMapLibre Map
   p->map_->resize(size());
   p->map_->setFramebufferObject(defaultFramebufferObject(),
                                 size() * pixelRatio());
   p->map_->render();

   // Perform mouse picking
   if (p->hasMouse_)
   {
      p->RunMousePicking();
   }
   else if (p->lastItemPicked_)
   {
      // Hide the tooltip when losing focus
      util::tooltip::Hide();

      p->lastItemPicked_ = false;
   }

   // Pop default font
   ImGui::PopFont();

   // Render ImGui Frame
   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

   // Unlock ImGui font atlas after rendering
   imguiFontAtlasLock.unlock();

   // Paint complete
   Q_EMIT WidgetPainted();
}

void MapWidgetImpl::ImGuiCheckFonts()
{
   // Update ImGui Fonts if required
   std::uint64_t currentImGuiFontsBuildCount =
      manager::FontManager::Instance().imgui_fonts_build_count();

   if (imGuiFontsBuildCount_ != currentImGuiFontsBuildCount ||
       !model::ImGuiContextModel::Instance().font_atlas()->IsBuilt())
   {
      ImGui_ImplOpenGL3_DestroyFontsTexture();
      ImGui_ImplOpenGL3_CreateFontsTexture();
   }

   imGuiFontsBuildCount_ = currentImGuiFontsBuildCount;
}

void MapWidgetImpl::RunMousePicking()
{
   const QMapLibre::CustomLayerRenderParameters params =
      context_->render_parameters();

   auto coordinate = map_->coordinateForPixel(lastPos_);
   auto mouseScreenCoordinate =
      util::maplibre::LatLongToScreenCoordinate(coordinate);

   // For each layer in reverse
   bool                                 itemPicked   = false;
   std::shared_ptr<types::EventHandler> eventHandler = nullptr;
   for (auto it = genericLayers_.rbegin(); it != genericLayers_.rend(); ++it)
   {
      // Run mouse picking for each layer
      if ((*it)->RunMousePicking(params,
                                 lastPos_,
                                 lastGlobalPos_,
                                 mouseScreenCoordinate,
                                 {coordinate.first, coordinate.second},
                                 eventHandler))
      {
         // If a draw item was picked, don't process additional layers
         itemPicked = true;
         break;
      }
   }

   // If no draw item was picked, hide the tooltip
   if (!itemPicked)
   {
      util::tooltip::Hide();

      if (pickedEventHandler_ != nullptr)
      {
         // Send leave event to picked event handler
         if (pickedEventHandler_->event_ != nullptr)
         {
            QEvent event(QEvent::Type::Leave);
            pickedEventHandler_->event_(&event);
         }

         // Reset picked event handler
         pickedEventHandler_ = nullptr;
      }
   }
   else if (eventHandler != nullptr)
   {
      // If the event handler changed
      if (pickedEventHandler_ != eventHandler)
      {
         // Send leave event to old event handler
         if (pickedEventHandler_ != nullptr &&
             pickedEventHandler_->event_ != nullptr)
         {
            QEvent event(QEvent::Type::Leave);
            pickedEventHandler_->event_(&event);
         }

         // Send enter event to new event handler
         if (eventHandler->event_ != nullptr)
         {
            QEvent event(QEvent::Type::Enter);
            eventHandler->event_(&event);
         }

         // Store picked event handler
         pickedEventHandler_ = eventHandler;
      }
   }

   Q_EMIT widget_->MouseCoordinateChanged(
      {coordinate.first, coordinate.second});

   lastItemPicked_ = itemPicked;
}

void MapWidget::mapChanged(QMapLibre::Map::MapChange mapChange)
{
   switch (mapChange)
   {
   case QMapLibre::Map::MapChangeDidFinishLoadingStyle:
      p->UpdateLoadedStyle();
      p->AddLayers();
      break;

   default:
      break;
   }
}

void MapWidgetImpl::UpdateLoadedStyle()
{
   styleLayers_ = map_->layerIds();
}

void MapWidgetImpl::RadarProductManagerConnect()
{
   if (radarProductManager_ != nullptr)
   {
      connect(radarProductManager_.get(),
              &manager::RadarProductManager::Level3ProductsChanged,
              this,
              [this]() { Q_EMIT widget_->Level3ProductsChanged(); });

      connect(
         radarProductManager_.get(),
         &manager::RadarProductManager::NewDataAvailable,
         this,
         [this](common::RadarProductGroup             group,
                const std::string&                    product,
                std::chrono::system_clock::time_point latestTime)
         {
            if (autoRefreshEnabled_ &&
                context_->radar_product_group() == group &&
                (group == common::RadarProductGroup::Level2 ||
                 context_->radar_product() == product))
            {
               // Create file request
               std::shared_ptr<request::NexradFileRequest> request =
                  std::make_shared<request::NexradFileRequest>(
                     radarProductManager_->radar_id());

               // File request callback
               if (autoUpdateEnabled_)
               {
                  connect(
                     request.get(),
                     &request::NexradFileRequest::RequestComplete,
                     this,
                     [=,
                      this](std::shared_ptr<request::NexradFileRequest> request)
                     {
                        // Select loaded record
                        auto record = request->radar_product_record();

                        // Validate record, and verify current map context still
                        // displays product
                        if (record != nullptr &&
                            context_->radar_product_group() == group &&
                            (group == common::RadarProductGroup::Level2 ||
                             context_->radar_product() == product))
                        {
                           widget_->SelectRadarProduct(record);
                        }
                     });
               }

               // Load file
               boost::asio::post(
                  threadPool_,
                  [=, this]()
                  {
                     if (group == common::RadarProductGroup::Level2)
                     {
                        radarProductManager_->LoadLevel2Data(latestTime,
                                                             request);
                     }
                     else
                     {
                        radarProductManager_->LoadLevel3Data(
                           product, latestTime, request);
                     }
                  });
            }
         },
         Qt::QueuedConnection);
   }
}

void MapWidgetImpl::RadarProductManagerDisconnect()
{
   if (radarProductManager_ != nullptr)
   {
      disconnect(radarProductManager_.get(),
                 &manager::RadarProductManager::NewDataAvailable,
                 this,
                 nullptr);
   }
}

void MapWidgetImpl::InitializeNewRadarProductView(
   const std::string& colorPalette)
{
   boost::asio::post(threadPool_,
                     [=, this]()
                     {
                        auto radarProductView = context_->radar_product_view();

                        std::string colorTableFile =
                           settings::PaletteSettings::Instance()
                              .palette(colorPalette)
                              .GetValue();
                        if (!colorTableFile.empty())
                        {
                           std::unique_ptr<std::istream> colorTableStream =
                              util::OpenFile(colorTableFile);
                           std::shared_ptr<common::ColorTable> colorTable =
                              common::ColorTable::Load(*colorTableStream);
                           radarProductView->LoadColorTable(colorTable);
                        }

                        radarProductView->Initialize();
                     });

   if (map_ != nullptr)
   {
      AddLayers();
   }
}

void MapWidgetImpl::RadarProductViewConnect()
{
   auto radarProductView = context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      connect(
         radarProductView.get(),
         &view::RadarProductView::ColorTableLutUpdated,
         this,
         [this]() { widget_->update(); },
         Qt::QueuedConnection);
      connect(
         radarProductView.get(),
         &view::RadarProductView::SweepComputed,
         this,
         [=, this]()
         {
            std::shared_ptr<config::RadarSite> radarSite =
               radarProductManager_->radar_site();

            RadarRangeLayer::Update(
               map_,
               radarProductView->range(),
               {radarSite->latitude(), radarSite->longitude()});
            widget_->update();
            Q_EMIT widget_->RadarSweepUpdated();
         },
         Qt::QueuedConnection);
      connect(radarProductView.get(),
              &view::RadarProductView::SweepNotComputed,
              widget_,
              &MapWidget::RadarSweepNotUpdated);
   }
}

void MapWidgetImpl::RadarProductViewDisconnect()
{
   auto radarProductView = context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      disconnect(radarProductView.get(),
                 &view::RadarProductView::ColorTableLutUpdated,
                 this,
                 nullptr);
      disconnect(radarProductView.get(),
                 &view::RadarProductView::SweepComputed,
                 this,
                 nullptr);
      disconnect(radarProductView.get(),
                 &view::RadarProductView::SweepNotComputed,
                 widget_,
                 nullptr);
   }
}

void MapWidgetImpl::SetRadarSite(const std::string& radarSite)
{
   // Check if radar site has changed
   if (radarProductManager_ == nullptr ||
       radarSite != radarProductManager_->radar_site()->id())
   {
      // Disconnect signals from old RadarProductManager
      RadarProductManagerDisconnect();

      // Set new RadarProductManager
      radarProductManager_ = manager::RadarProductManager::Instance(radarSite);

      // Update views
      context_->overlay_product_view()->set_radar_product_manager(
         radarProductManager_);

      // Connect signals to new RadarProductManager
      RadarProductManagerConnect();

      radarProductManager_->UpdateAvailableProducts();
   }
}

void MapWidgetImpl::Update()
{
   widget_->update();

   if (UpdateStoredMapParameters())
   {
      Q_EMIT widget_->MapParametersChanged(
         prevLatitude_, prevLongitude_, prevZoom_, prevBearing_, prevPitch_);
   }
}

bool MapWidgetImpl::UpdateStoredMapParameters()
{
   bool changed = false;

   double newLatitude  = map_->latitude();
   double newLongitude = map_->longitude();
   double newZoom      = map_->zoom();
   double newBearing   = map_->bearing();
   double newPitch     = map_->pitch();

   if (prevLatitude_ != newLatitude ||   //
       prevLongitude_ != newLongitude || //
       prevZoom_ != newZoom ||           //
       prevBearing_ != newBearing ||     //
       prevPitch_ != newPitch)
   {
      prevLatitude_  = newLatitude;
      prevLongitude_ = newLongitude;
      prevZoom_      = newZoom;
      prevBearing_   = newBearing;
      prevPitch_     = newPitch;

      changed = true;
   }

   return changed;
}

} // namespace map
} // namespace qt
} // namespace scwx

#include "map_widget.moc"
