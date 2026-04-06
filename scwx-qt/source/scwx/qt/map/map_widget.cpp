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
#include <scwx/qt/map/marker_layer.hpp>
#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/map/overlay_product_layer.hpp>
#include <scwx/qt/map/placefile_layer.hpp>
#include <scwx/qt/map/radar_product_layer.hpp>
#include <scwx/qt/map/radar_range_layer.hpp>
#include <scwx/qt/map/radar_site_layer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/ui/edit_marker_dialog.hpp>
#include <scwx/qt/util/file.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/view/radar_product_view_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <limits>
#include <ranges>
#include <set>
#include <utility>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/range/join.hpp>
#include <boost/uuid/random_generator.hpp>
#include <fmt/chrono.h>
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
#include <QPinchGesture>
#include <QString>
#include <QTextDocument>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::map_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr double kDefaultZoom_ {7.0};

class MapWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit MapWidgetImpl(MapWidget*                     widget,
                          std::size_t                    id,
                          QMapLibre::Settings            settings,
                          std::shared_ptr<gl::GlContext> glContext) :
       id_ {id},
       uuid_ {boost::uuids::random_generator()()},
       glContext_ {std::move(glContext)},
       widget_ {widget},
       settings_(std::move(settings)),
       map_(),
       layerList_ {},
       imGuiRendererInitialized_ {false},
       radarProductManager_ {nullptr},
       radarProductLayer_ {nullptr},
       overlayLayer_ {nullptr},
       placefileLayer_ {nullptr},
       markerLayer_ {nullptr},
       colorTableLayer_ {nullptr},
       autoRefreshEnabled_ {true},
       autoUpdateEnabled_ {true},
       selectedLevel2Product_ {common::Level2Product::Unknown},
       currentStyleIndex_ {0},
       currentStyle_ {nullptr},
       frameDraws_(0),
       tiltsToIndices_ {}
   {
      // Create views
      auto overlayProductView = std::make_shared<view::OverlayProductView>();
      overlayProductView->SetAutoRefresh(autoRefreshEnabled_);
      overlayProductView->SetAutoUpdate(autoUpdateEnabled_);

      // Initialize AlertLayerHandler
      map::AlertLayer::InitializeHandler();

      auto& generalSettings = settings::GeneralSettings::Instance();
      auto& mapSettings     = settings::MapSettings::Instance();

      // Initialize context
      context_->set_map_provider(
         GetMapProvider(generalSettings.map_provider().GetValue()));
      context_->set_overlay_product_view(overlayProductView);
      context_->set_widget(widget);

      // Initialize map data
      SetRadarSite(generalSettings.default_radar_site().GetValue());
      smoothingEnabled_ = mapSettings.smoothing_enabled(id).GetValue();

      // Create ImGui Context
      static size_t currentMapId_ {0u};
      imGuiContextName_ = fmt::format("Map {}", ++currentMapId_);
      imGuiContext_ =
         model::ImGuiContextModel::Instance().CreateContext(imGuiContextName_);

      // Initialize ImGui Qt backend
      ImGui_ImplQt_Init();

      InitializeCustomStyles();
   }

   ~MapWidgetImpl()
   {
      // Disconnect signals
      colorPaletteConnection_.disconnect();
      for (auto& connection : connections_)
      {
         connection.disconnect();
      }

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
   void AddLayer(const std::string&                   id,
                 const std::shared_ptr<GenericLayer>& layer,
                 const std::string&                   before = {});
   void AddLayers();
   void AddPlacefileLayer(const std::string& placefileName,
                          const std::string& before);
   void ConnectMapSignals();
   void ConnectSignals();
   void HandleHotkeyPressed(types::Hotkey hotkey, bool isAutoRepeat);
   void HandleHotkeyReleased(types::Hotkey hotkey);
   void HandleHotkeyUpdates();
   void HandlePinchGesture(QPinchGesture* gesture);
   void InitializeCustomStyles();
   void InitializeNewRadarProductView(const std::string& colorPalette);
   void RadarProductManagerConnect();
   void RadarProductManagerDisconnect();
   void RadarProductViewConnect();
   void RadarProductViewDisconnect();
   void ResetMap(const std::string& styleName);
   [[nodiscard]] std::string
        ResolveMapStyleName(const std::string& preferredStyleName) const;
   void RunMousePicking();
   void ScreenCaptureCopy();
   void ScreenCaptureSaveImage();
   void SelectNearestRadarSite(double                     latitude,
                               double                     longitude,
                               std::optional<std::string> type);
   void SetRadarSite(const std::string& radarSite,
                     bool               checkProductAvailability = false);
   void UpdateColorTable(const std::string& colorPalette);
   void UpdateLoadedStyle();
   bool UpdateStoredMapParameters();
   void CheckLevel3Availability();

   common::Level2Product
   GetLevel2ProductOrDefault(const std::string& productName) const;

   static std::string GetPlacefileLayerName(const std::string& placefileName);

   boost::asio::thread_pool threadPool_ {2u};

   std::size_t        id_;
   boost::uuids::uuid uuid_;

   std::shared_ptr<MapContext>    context_ {std::make_shared<MapContext>()};
   std::shared_ptr<gl::GlContext> glContext_;

   MapWidget*                      widget_;
   QMapLibre::Settings             settings_;
   std::shared_ptr<QMapLibre::Map> map_;
   std::list<std::string>          layerList_;

   std::vector<std::shared_ptr<GenericLayer>> genericLayers_ {};

   const std::vector<MapStyle> emptyStyles_ {};
   const std::vector<MapStyle> noneStyles_ {
      MapStyle {.name_ {"None"}, .url_ {}, .drawBelow_ {}}};
   std::vector<MapStyle> customStyles_ {
      MapStyle {.name_ {"Custom"}, .url_ {}, .drawBelow_ {}}};
   QStringList styleLayers_;

   std::vector<boost::signals2::scoped_connection> connections_ {};
   boost::signals2::scoped_connection              colorPaletteConnection_ {};

   ImGuiContext* imGuiContext_;
   std::string   imGuiContextName_;
   bool          imGuiRendererInitialized_ {false};

   std::shared_ptr<model::LayerModel> layerModel_ {
      model::LayerModel::Instance()};

   ui::EditMarkerDialog* editMarkerDialog_ {nullptr};

   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};
   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};
   std::shared_ptr<manager::RadarProductManager> radarProductManager_;

   std::shared_ptr<RadarProductLayer>   radarProductLayer_;
   std::shared_ptr<OverlayLayer>        overlayLayer_;
   std::shared_ptr<OverlayProductLayer> overlayProductLayer_ {nullptr};
   std::shared_ptr<PlacefileLayer>      placefileLayer_;
   std::shared_ptr<MarkerLayer>         markerLayer_;
   std::shared_ptr<ColorTableLayer>     colorTableLayer_;
   std::shared_ptr<RadarSiteLayer>      radarSiteLayer_ {nullptr};

   std::list<std::shared_ptr<PlacefileLayer>> placefileLayers_ {};

   bool autoRefreshEnabled_;
   bool autoUpdateEnabled_;
   bool smoothingEnabled_ {false};

   common::Level2Product selectedLevel2Product_;

   bool            hasMouse_ {false};
   bool            isPainting_ {false};
   bool            lastItemPicked_ {false};
   QPointF         lastPos_ {};
   QPointF         lastGlobalPos_ {};
   std::size_t     currentStyleIndex_;
   const MapStyle* currentStyle_;
   std::string     initialStyleName_ {};
   bool            mapChangedOnce_ {false};
   bool            mapStylePending_ {false};

   Qt::KeyboardModifiers lastKeyboardModifiers_ {
      Qt::KeyboardModifier::NoModifier};

   std::weak_ptr<types::EventHandler> weakPickedEventHandler_ {};

   uint64_t frameDraws_;

   double prevLatitude_ {0.0};
   double prevLongitude_ {0.0};
   double prevZoom_ {kDefaultZoom_};
   double prevBearing_ {0.0};
   double prevPitch_ {0.0};

   types::CaptureType screenCaptureRequested_ {types::CaptureType::None};

   std::set<types::Hotkey>               activeHotkeys_ {};
   std::chrono::system_clock::time_point prevHotkeyTime_ {};

   bool productAvailabilityCheckNeeded_ {false};
   bool productAvailabilityUpdated_ {false};
   bool productAvailabilityProductSelected_ {false};

   std::unordered_map<std::string, size_t> tiltsToIndices_;
   size_t                                  currentTiltIndex_ {0};

public slots:
   void Update();
};

MapWidget::MapWidget(std::size_t                    id,
                     const QMapLibre::Settings&     settings,
                     std::shared_ptr<gl::GlContext> glContext) :
    p(std::make_unique<MapWidgetImpl>(this, id, settings, std::move(glContext)))
{
   if (settings::GeneralSettings::Instance().anti_aliasing_enabled().GetValue())
   {
      QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
      surfaceFormat.setSamples(4);
      setFormat(surfaceFormat);
   }

   setFocusPolicy(Qt::StrongFocus);

   grabGesture(Qt::GestureType::PinchGesture);

   ImGui_ImplQt_RegisterWidget(this);

   // Qt parent deals with memory management
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
   p->editMarkerDialog_ = new ui::EditMarkerDialog(this);

   p->ConnectSignals();
}

MapWidget::~MapWidget()
{
   // Make sure we have a valid context so we can delete the QMapLibre.
   makeCurrent();
}

void MapWidgetImpl::InitializeCustomStyles()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   auto& customStyleUrl       = generalSettings.custom_style_url();
   auto& customStyleDrawLayer = generalSettings.custom_style_draw_layer();

   auto& customStyle = customStyles_.at(0);
   customStyle.url_  = customStyleUrl.GetValue();
   customStyle.drawBelow_.push_back(customStyleDrawLayer.GetValue());

   connections_.emplace_back(customStyleUrl.changed_signal().connect(
      [this](const auto& event) { customStyles_[0].url_ = event.newValue_; }));
   connections_.emplace_back(customStyleDrawLayer.changed_signal().connect(
      [this](const auto& event)
      {
         const std::string& drawLayer = event.newValue_;
         if (!drawLayer.empty())
         {
            customStyles_[0].drawBelow_ = {drawLayer};
         }
         else
         {
            customStyles_[0].drawBelow_.clear();
         }
      }));
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

              // HTML cannot currently be included in ImGui windows. Where
              // links can't be included, remove "Improve this map".
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
           static_cast<void (QWidget::*)()>(&QWidget::update));

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
           &MapWidgetImpl::HandleHotkeyPressed);
   connect(hotkeyManager_.get(),
           &manager::HotkeyManager::HotkeyReleased,
           this,
           &MapWidgetImpl::HandleHotkeyReleased);
   connect(widget_,
           &MapWidget::RadarSiteUpdated,
           widget_,
           [this](const std::shared_ptr<config::RadarSite>&)
           {
              productAvailabilityProductSelected_ = true;
              CheckLevel3Availability();
           });

   auto& generalSettings = settings::GeneralSettings::Instance();

   connections_.emplace_back(
      generalSettings.map_provider().changed_signal().connect(
         [this](const auto& event)
         {
            const auto mapProvider = GetMapProvider(event.newValue_);
            context_->set_map_provider(mapProvider);
            ConfigureMapSettings(mapProvider, settings_);

            const std::string styleName = ResolveMapStyleName(
               currentStyle_ ? currentStyle_->name_ : initialStyleName_);

            initialStyleName_ = styleName;
            ResetMap(styleName);
         }));
   connections_.emplace_back(
      generalSettings.mapbox_api_key().changed_signal().connect(
         [this](const auto& event)
         {
            if (context_->map_provider() == MapProvider::Mapbox)
            {
               // Reset the map, since the API key is embedded in settings
               settings_.setApiKey(QString::fromStdString(event.newValue_));
               const std::string& activeStyleName =
                  currentStyle_ ? currentStyle_->name_ : initialStyleName_;
               ResetMap(activeStyleName == "None" ? "" : activeStyleName);
            }
         }));
   connections_.emplace_back(
      generalSettings.maptiler_api_key().changed_signal().connect(
         [this](auto&&...)
         {
            if (context_->map_provider() == MapProvider::MapTiler)
            {
               // Reapply style instead of resetting the map
               const std::string& activeStyleName =
                  currentStyle_ ? currentStyle_->name_ : initialStyleName_;
               widget_->SetMapStyle(activeStyleName == "None" ?
                                       ResolveMapStyleName("") :
                                       activeStyleName,
                                    true);
            }
         }));
}

void MapWidgetImpl::HandleHotkeyPressed(types::Hotkey hotkey, bool isAutoRepeat)
{
   Q_UNUSED(isAutoRepeat);

   switch (hotkey)
   {
   case types::Hotkey::AddLocationMarker:
      if (hasMouse_)
      {
         auto coordinate = map_->coordinateForPixel(lastPos_);

         editMarkerDialog_->setup(coordinate.first, coordinate.second);
         editMarkerDialog_->show();
      }
      break;

   case types::Hotkey::ChangeMapStyle:
      if (context_->settings().isActive_)
      {
         widget_->changeStyle();
      }
      break;

   case types::Hotkey::CopyCursorCoordinates:
      if (hasMouse_)
      {
         QClipboard* clipboard  = QGuiApplication::clipboard();
         auto        coordinate = map_->coordinateForPixel(lastPos_);
         std::string text =
            fmt::format("{}, {}", coordinate.first, coordinate.second);
         clipboard->setText(QString::fromStdString(text));
      }
      break;

   case types::Hotkey::CopyMapCoordinates:
      if (context_->settings().isActive_)
      {
         QClipboard* clipboard  = QGuiApplication::clipboard();
         auto        coordinate = map_->coordinate();
         std::string text =
            fmt::format("{}, {}", coordinate.first, coordinate.second);
         clipboard->setText(QString::fromStdString(text));
      }
      break;

   case types::Hotkey::ScreenCaptureCopy:
      Q_EMIT widget_->ScreenCaptureRequested(types::CaptureType::Copy);
      break;

   case types::Hotkey::ScreenCaptureSaveImage:
      Q_EMIT widget_->ScreenCaptureRequested(types::CaptureType::SaveImage);
      break;

   default:
      break;
   }

   activeHotkeys_.insert(hotkey);
}

void MapWidgetImpl::HandleHotkeyReleased(types::Hotkey hotkey)
{
   // Erase the hotkey from the active set regardless of whether this is the
   // active map
   activeHotkeys_.erase(hotkey);
}

void MapWidgetImpl::HandleHotkeyUpdates()
{
   using namespace std::chrono_literals;

   static constexpr float  kMapPanFactor    = 0.2f;
   static constexpr float  kMapRotateFactor = 0.2f;
   static constexpr double kMapScaleFactor  = 1000.0;

   std::chrono::system_clock::time_point hotkeyTime =
      std::chrono::system_clock::now();
   std::chrono::milliseconds hotkeyElapsed =
      std::min(std::chrono::duration_cast<std::chrono::milliseconds>(
                  hotkeyTime - prevHotkeyTime_),
               100ms);

   prevHotkeyTime_ = hotkeyTime;

   if (!context_->settings().isActive_)
   {
      // Don't attempt to handle a hotkey if this is not the active map
      return;
   }

   for (auto& hotkey : activeHotkeys_)
   {
      switch (hotkey)
      {
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
         QPointF center     = {widgetSize.width() * 0.5f,
                               widgetSize.height() * 0.5f};
         double  scale = std::pow(2.0, hotkeyElapsed.count() / kMapScaleFactor);
         map_->scaleBy(scale, center);
         break;
      }

      case types::Hotkey::MapZoomOut:
      {
         auto    widgetSize = widget_->size();
         QPointF center     = {widgetSize.width() * 0.5f,
                               widgetSize.height() * 0.5f};
         double  scale =
            1.0 / std::pow(2.0, hotkeyElapsed.count() / kMapScaleFactor);
         map_->scaleBy(scale, center);
         break;
      }

      default:
         break;
      }
   }
}

void MapWidgetImpl::HandlePinchGesture(QPinchGesture* gesture)
{
   if (gesture->changeFlags() & QPinchGesture::ChangeFlag::ScaleFactorChanged)
   {
      map_->scaleBy(gesture->scaleFactor(),
                    widget_->mapFromGlobal(gesture->centerPoint()));
   }
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

std::optional<float> MapWidget::GetElevation() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->elevation();
   }
   else
   {
      return {};
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

std::optional<float> MapWidget::GetIncomingLevel2Elevation() const
{
   return p->radarProductManager_->incoming_level_2_elevation();
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

bool MapWidget::GetRadarWireframeEnabled() const
{
   return p->context_->settings().radarWireframeEnabled_;
}

void MapWidget::SetRadarWireframeEnabled(bool wireframeEnabled)
{
   p->context_->settings().radarWireframeEnabled_ = wireframeEnabled;
   QMetaObject::invokeMethod(
      this, static_cast<void (QWidget::*)()>(&QWidget::update));
}

bool MapWidget::GetSmoothingEnabled() const
{
   return p->smoothingEnabled_;
}

void MapWidget::SetSmoothingEnabled(bool smoothingEnabled)
{
   p->smoothingEnabled_ = smoothingEnabled;

   auto radarProductView = p->context_->radar_product_view();
   if (radarProductView != nullptr)
   {
      radarProductView->set_smoothing_enabled(smoothingEnabled);
      radarProductView->Update();
   }
}

std::optional<float> MapWidget::GetColorTableThreshold() const
{
   auto radarProductView = p->context_->radar_product_view();
   if (radarProductView != nullptr)
   {
      return radarProductView->color_table_threshold();
   }
   return std::nullopt;
}

std::pair<float, float> MapWidget::GetColorTableRange() const
{
   auto radarProductView = p->context_->radar_product_view();
   if (radarProductView != nullptr)
   {
      return radarProductView->GetColorTableRange();
   }
   return {-std::numeric_limits<float>::infinity(),
           std::numeric_limits<float>::infinity()};
}

std::string MapWidget::GetColorTableUnits() const
{
   auto radarProductView = p->context_->radar_product_view();
   if (radarProductView != nullptr)
   {
      return radarProductView->units();
   }
   return {};
}

void MapWidget::SetColorTableThreshold(std::optional<float> threshold)
{
   auto radarProductView = p->context_->radar_product_view();
   if (radarProductView != nullptr)
   {
      radarProductView->set_color_table_threshold(threshold);
   }
}

const scwx::util::time_zone* MapWidget::GetDefaultTimeZone() const
{
   return p->radarProductManager_->default_time_zone();
}

void MapWidget::ScreenCapture(types::CaptureType captureType)
{
   p->screenCaptureRequested_ = captureType;
   QMetaObject::invokeMethod(
      this, static_cast<void (QWidget::*)()>(&QWidget::update));
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

   if (group == common::RadarProductGroup::Level3)
   {
      const auto& tiltIndex = p->tiltsToIndices_.find(productName);
      p->currentTiltIndex_ =
         tiltIndex != p->tiltsToIndices_.cend() ? tiltIndex->second : 0;
   }
   else
   {
      p->currentTiltIndex_ = 0;
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
      radarProductView->set_smoothing_enabled(p->smoothingEnabled_);
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

         auto& paletteSetting =
            settings::PaletteSettings::Instance().palette(palette);

         p->colorPaletteConnection_ = paletteSetting.changed_signal().connect(
            [this, palette](auto&&...) { p->UpdateColorTable(palette); });

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
      p->SetRadarSite(radarSite->id(), true);
      p->Update();

      // Select products from new site
      if (radarProductView != nullptr)
      {
         radarProductView->set_radar_product_manager(p->radarProductManager_);
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
   QMetaObject::invokeMethod(
      this, static_cast<void (QWidget::*)()>(&QWidget::update));
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
         auto& generalSettings = settings::GeneralSettings::Instance();

         // Find the nearest radar
         std::optional<std::string> type = std::nullopt;

         if (generalSettings.auto_navigate_to_wsr88d_only().GetValue())
         {
            // Find the nearest WSR-88D radar
            type = "wsr88d";
         }

         // Find the nearest radar
         const std::shared_ptr<config::RadarSite> nearestRadarSite =
            config::RadarSite::FindNearest(latitude, longitude, type);

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

void MapWidget::SetMapStyle(const std::string& styleName, bool force)
{
   const auto  mapProvider     = p->context_->map_provider();
   const auto& mapProviderInfo = GetMapProviderInfo(mapProvider);
   auto&       fixedStyles     = mapProviderInfo.mapStyles_;

   auto styles = boost::join(boost::join(fixedStyles, p->noneStyles_),
                             p->customStyles_[0].IsValid() ? p->customStyles_ :
                                                             p->emptyStyles_);

   for (size_t i = 0u; i < styles.size(); ++i)
   {
      const auto* style = &styles[static_cast<std::ptrdiff_t>(i)];

      if (style->name_ == styleName)
      {
         if (p->currentStyleIndex_ == i && p->currentStyle_ == style && !force)
         {
            // No need to set the style again
            break;
         }

         p->currentStyleIndex_ = i;
         p->currentStyle_      = style;

         logger_->debug("Updating style: {}", style->name_);

         util::maplibre::SetMapStyleUrl(p->context_, style->url_);

         break;
      }
   }
}

void MapWidget::UpdateMouseCoordinate(const common::Coordinate& coordinate)
{
   if (p->context_->mouse_coordinate() != coordinate)
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      p->context_->set_mouse_coordinate(coordinate);

      auto keyboardModifiers = QGuiApplication::keyboardModifiers();

      if (generalSettings.cursor_icon_always_on().GetValue() ||
          keyboardModifiers != Qt::KeyboardModifier::NoModifier ||
          keyboardModifiers != p->lastKeyboardModifiers_)
      {
         QMetaObject::invokeMethod(
            this, static_cast<void (QWidget::*)()>(&QWidget::update));
      }

      p->lastKeyboardModifiers_ = keyboardModifiers;
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
   auto&       fixedStyles     = mapProviderInfo.mapStyles_;

   auto styles = boost::join(boost::join(fixedStyles, p->noneStyles_),
                             p->customStyles_[0].IsValid() ? p->customStyles_ :
                                                             p->emptyStyles_);

   if (++p->currentStyleIndex_ >= styles.size())
   {
      p->currentStyleIndex_ = 0;
   }

   p->currentStyle_ =
      &styles[static_cast<std::ptrdiff_t>(p->currentStyleIndex_)];

   logger_->debug("Updating style: {}", p->currentStyle_->name_);

   util::maplibre::SetMapStyleUrl(p->context_, p->currentStyle_->url_);

   Q_EMIT MapStyleChanged(p->currentStyle_->name_);
}

void MapWidget::DumpLayerList() const
{
   logger_->info("Layers: {}", p->map_->layerIds().join(", ").toStdString());
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
   types::LayerVector customLayers = model::LayerModel::Instance()->GetLayers();

   // Start by drawing layers before any style-defined layers
   std::string before = styleLayers_.front().toStdString();

   // Loop through each custom layer in reverse order
   for (const auto& customLayer : std::ranges::reverse_view(customLayers))
   {
      if (customLayer.type_ == types::LayerType::Map)
      {
         // Style-defined map layers
         switch (std::get<types::MapLayer>(customLayer.description_))
         {
         // Subsequent layers are drawn underneath the map symbology layer
         case types::MapLayer::MapUnderlay:
            before = util::maplibre::FindMapSymbologyLayer(
               styleLayers_, currentStyle_->drawBelow_);
            break;

         // Subsequent layers are drawn after all style-defined layers
         case types::MapLayer::MapSymbology:
            before = "";
            break;

         default:
            break;
         }
      }
      // id_ is always < 4, so this is safe
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      else if (customLayer.displayed_[id_])
      {
         // If the layer is displayed for the current map, add it
         AddLayer(customLayer.type_, customLayer.description_, before);
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
         radarProductLayer_ = std::make_shared<RadarProductLayer>(glContext_);
         AddLayer(layerName, radarProductLayer_, before);
      }
   }
   else if (type == types::LayerType::Alert)
   {
      auto phenomenon = std::get<awips::Phenomenon>(description);

      std::shared_ptr<AlertLayer> alertLayer =
         std::make_shared<AlertLayer>(glContext_, phenomenon);
      AddLayer(fmt::format("alert.{}", awips::GetPhenomenonCode(phenomenon)),
               alertLayer,
               before);
      connect(alertLayer.get(),
              &AlertLayer::AlertSelected,
              widget_,
              &MapWidget::AlertSelected);
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
         overlayLayer_ = std::make_shared<OverlayLayer>(glContext_);
         AddLayer(layerName, overlayLayer_, before);
         break;

      // If there is a radar product view, create the color table layer
      case types::InformationLayer::ColorTable:
         if (radarProductView != nullptr)
         {
            colorTableLayer_ = std::make_shared<ColorTableLayer>(glContext_);
            AddLayer(layerName, colorTableLayer_, before);
         }
         break;

      // Create the radar site layer
      case types::InformationLayer::RadarSite:
         radarSiteLayer_ = std::make_shared<RadarSiteLayer>(glContext_);
         AddLayer(layerName, radarSiteLayer_, before);
         connect(
            radarSiteLayer_.get(),
            &RadarSiteLayer::RadarSiteSelected,
            this,
            [this](const std::string& id)
            {
               auto& generalSettings = settings::GeneralSettings::Instance();
               widget_->RadarSiteRequested(
                  id, generalSettings.center_on_radar_selection().GetValue());
            });
         break;

      // Create the location marker layer
      case types::InformationLayer::Markers:
         markerLayer_ = std::make_shared<MarkerLayer>(glContext_);
         AddLayer(layerName, markerLayer_, before);
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
               std::make_shared<OverlayProductLayer>(glContext_);
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
      std::make_shared<PlacefileLayer>(glContext_, placefileName);
   placefileLayers_.push_back(placefileLayer);
   AddLayer(GetPlacefileLayerName(placefileName), placefileLayer, before);

   // When the layer updates, trigger a map widget update
   connect(placefileLayer.get(),
           &PlacefileLayer::DataReloaded,
           widget_,
           static_cast<void (QWidget::*)()>(&QWidget::update));
}

std::string
MapWidgetImpl::GetPlacefileLayerName(const std::string& placefileName)
{
   return types::GetLayerName(types::LayerType::Placefile, placefileName);
}

void MapWidgetImpl::AddLayer(const std::string&                   id,
                             const std::shared_ptr<GenericLayer>& layer,
                             const std::string&                   before)
{
   // QMapLibre::addCustomLayer will take ownership of the std::unique_ptr
   std::unique_ptr<QMapLibre::CustomLayerHostInterface> pHost =
      std::make_unique<LayerWrapper>(layer, context_);

   try
   {
      map_->addCustomLayer(id.c_str(), std::move(pHost), before.c_str());

      layerList_.push_back(id);
      genericLayers_.push_back(layer);

      connect(layer.get(),
              &GenericLayer::NeedsRendering,
              widget_,
              static_cast<void (QWidget::*)()>(&QWidget::update));
   }
   catch (const std::exception&)
   {
      // When dragging and dropping, a temporary duplicate layer exists
   }
}

bool MapWidget::event(QEvent* e)
{
   if (e->type() == QEvent::Type::Paint && p->isPainting_)
   {
      logger_->error("Recursive paint event ignored");

      // Ignore recursive paint events
      return true;
   }

   auto pickedEventHandler = p->weakPickedEventHandler_.lock();
   if (pickedEventHandler != nullptr && pickedEventHandler->event_ != nullptr)
   {
      pickedEventHandler->event_(e);
   }
   pickedEventHandler.reset();

   switch (e->type())
   {
   case QEvent::Type::Gesture:
      // QEvent is always a QGestureEvent
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
      gestureEvent(static_cast<QGestureEvent*>(e));
      break;

   default:
      break;
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
   if (p->hotkeyManager_->HandleKeyPress(ev))
   {
      ev->accept();
   }
}

void MapWidget::keyReleaseEvent(QKeyEvent* ev)
{
   if (p->hotkeyManager_->HandleKeyRelease(ev))
   {
      ev->accept();
   }
}

void MapWidget::gestureEvent(QGestureEvent* ev)
{
   if (QGesture* pinch = ev->gesture(Qt::PinchGesture))
   {
      // QGesture is always a QPinchGesture
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
      p->HandlePinchGesture(static_cast<QPinchGesture*>(pinch));
   }
}

void MapWidget::mousePressEvent(QMouseEvent* ev)
{
   p->lastPos_       = ev->position();
   p->lastGlobalPos_ = ev->globalPosition();

   if (ev->type() == QEvent::Type::MouseButtonPress)
   {
      if (ev->buttons() ==
          (Qt::MouseButton::LeftButton | Qt::MouseButton::RightButton))
      {
         changeStyle();
      }
      else if (ev->buttons() == Qt::MouseButton::MiddleButton)
      {
         auto& generalSettings = settings::GeneralSettings::Instance();

         // Select nearest radar on middle click
         std::optional<std::string> type = std::nullopt;

         if (generalSettings.auto_navigate_to_wsr88d_only().GetValue())
         {
            // Select nearest WSR-88D radar on middle click
            type = "wsr88d";
         }

         auto coordinate = p->map_->coordinateForPixel(p->lastPos_);
         p->SelectNearestRadarSite(coordinate.first, coordinate.second, type);
      }
   }

   if (ev->type() == QEvent::Type::MouseButtonDblClick)
   {
      if (ev->buttons() == Qt::MouseButton::LeftButton)
      {
         p->map_->scaleBy(2.0, p->lastPos_);
      }
      else if (ev->buttons() == Qt::MouseButton::RightButton)
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
      if (ev->buttons() == Qt::MouseButton::LeftButton)
      {
         p->map_->moveBy(delta);
      }
      else if (ev->buttons() == Qt::MouseButton::RightButton)
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

   p->glContext_->Initialize();

   // Lock ImGui font atlas prior to new ImGui frame
   std::shared_lock imguiFontAtlasLock {
      manager::FontManager::Instance().imgui_font_atlas_mutex()};

   // Initialize ImGui OpenGL3 backend
   ImGui::SetCurrentContext(p->imGuiContext_);
   ImGui_ImplQt_RegisterWidget(this);
   ImGui_ImplOpenGL3_Init();
   p->imGuiRendererInitialized_ = true;

   p->ResetMap(p->initialStyleName_);

   p->UpdateStoredMapParameters();
   Q_EMIT MapParametersChanged(p->prevLatitude_,
                               p->prevLongitude_,
                               p->prevZoom_,
                               p->prevBearing_,
                               p->prevPitch_);
}

void MapWidgetImpl::ResetMap(const std::string& styleName)
{
   logger_->debug("Resetting map");

   const std::string resolvedStyleName = ResolveMapStyleName(styleName);

   initialStyleName_ = resolvedStyleName;

   // Determine whether this is the first-time initialization or a runtime reset
   const bool hadExistingMap = static_cast<bool>(map_);

   map_ = std::make_shared<QMapLibre::Map>(
      nullptr, settings_, widget_->size(), widget_->pixelRatio());
   context_->set_map(map_);
   ConnectMapSignals();

   // Set initial location:
   //  - On first initialization, center on the radar site.
   //  - On subsequent resets, restore the previous map view position.
   if (hadExistingMap)
   {
      map_->setCoordinateZoom({prevLatitude_, prevLongitude_}, prevZoom_);
   }
   else
   {
      const std::shared_ptr<config::RadarSite> radarSite =
         radarProductManager_->radar_site();
      map_->setCoordinateZoom({radarSite->latitude(), radarSite->longitude()},
                              prevZoom_);
   }

   // Update style
   if (resolvedStyleName.empty())
   {
      widget_->changeStyle();
   }
   else
   {
      widget_->SetMapStyle(resolvedStyleName, true);

      if (resolvedStyleName == "None" || resolvedStyleName == "Custom")
      {
         // An empty map style may not trigger a map change event, so set the
         // pending flag to ensure the map style is applied to the map layers
         mapStylePending_ = true;
      }
   }

   mapChangedOnce_ = false;

   connect(
      map_.get(), &QMapLibre::Map::mapChanged, widget_, &MapWidget::mapChanged);
   connect(map_.get(),
           &QMapLibre::Map::mapLoadingFailed,
           widget_,
           [this](QMapLibre::Map::MapLoadingFailure, const QString& reason)
           {
              logger_->error("Map loading failed: {}", reason.toStdString());

              // If the map failed to load, and we haven't loaded a map yet,
              // default to the "None" map. This prevents a "black screen" on
              // startup.
              if (!mapChangedOnce_)
              {
                 mapChangedOnce_ = true;
                 widget_->SetMapStyle("None");
              }
           });
}

std::string
MapWidgetImpl::ResolveMapStyleName(const std::string& preferredStyleName) const
{
   const auto& mapProviderInfo = GetMapProviderInfo(context_->map_provider());

   if ((customStyles_[0].IsValid() && preferredStyleName == "Custom") ||
       preferredStyleName == "None" ||
       std::ranges::find_if(mapProviderInfo.mapStyles_,
                            [&](const auto& mapStyle)
                            { return mapStyle.name_ == preferredStyleName; }) !=
          mapProviderInfo.mapStyles_.cend())
   {
      return preferredStyleName;
   }

   return !mapProviderInfo.mapStyles_.empty() ?
             mapProviderInfo.mapStyles_.front().name_ :
             "None";
}

void MapWidget::paintGL()
{
   // Check for screen capture
   const types::CaptureType currentCaptureType = p->screenCaptureRequested_;
   if (p->screenCaptureRequested_ != types::CaptureType::None)
   {
      p->screenCaptureRequested_ = types::CaptureType::None;
      p->context_->set_screen_capture(true);
   }

   p->isPainting_ = true;

   auto defaultFont = manager::FontManager::Instance().GetImGuiFont(
      types::FontCategory::Default);

   p->frameDraws_++;

   p->glContext_->StartFrame();

   // Handle hotkey updates
   p->HandleHotkeyUpdates();

   // Lock ImGui font atlas prior to new ImGui frame
   std::shared_lock imguiFontAtlasLock {
      manager::FontManager::Instance().imgui_font_atlas_mutex()};

   // Update pixel ratio
   p->context_->set_pixel_ratio(pixelRatio());

   // Render QMapLibre Map
   p->map_->resize(size());
   p->map_->setOpenGLFramebufferObject(defaultFramebufferObject(),
                                       size() * pixelRatio());
   p->map_->render();

   // ImGui tool tip code
   // Setup ImGui Frame
   ImGui::SetCurrentContext(p->imGuiContext_);

   // Start ImGui Frame
   model::ImGuiContextModel::Instance().NewFrame();
   ImGui_ImplQt_NewFrame(this);
   ImGui_ImplOpenGL3_NewFrame();
   ImGui::NewFrame();

   // Set default font
   ImGui::PushFont(defaultFont.first->font(), defaultFont.second.value());

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

   p->isPainting_ = false;

   // Screen capture post-processing
   if (currentCaptureType != types::CaptureType::None)
   {
      switch (currentCaptureType)
      {
      case types::CaptureType::Copy:
         p->ScreenCaptureCopy();
         break;

      case types::CaptureType::SaveImage:
         p->ScreenCaptureSaveImage();
         break;

      default:
         break;
      }

      // Clear screen capture
      p->context_->set_screen_capture(false);

      // Queue another update
      update();
   }
}

void MapWidgetImpl::RunMousePicking()
{
   const QMapLibre::CustomLayerRenderParameters params = {
      .width       = static_cast<double>(widget_->size().width()),
      .height      = static_cast<double>(widget_->size().height()),
      .latitude    = map_->coordinate().first,
      .longitude   = map_->coordinate().second,
      .zoom        = map_->zoom(),
      .bearing     = map_->bearing(),
      .pitch       = map_->pitch(),
      .fieldOfView = 0};

   auto coordinate = map_->coordinateForPixel(lastPos_);
   auto mouseScreenCoordinate =
      util::maplibre::LatLongToScreenCoordinate(coordinate);

   // For each layer in reverse
   bool                                 itemPicked   = false;
   std::shared_ptr<types::EventHandler> eventHandler = nullptr;
   for (auto it = genericLayers_.rbegin(); it != genericLayers_.rend(); ++it)
   {
      // Run mouse picking for each layer
      if ((*it)->RunMousePicking(context_,
                                 params,
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
   auto prevPickedEventHandler = weakPickedEventHandler_.lock();
   if (!itemPicked)
   {
      util::tooltip::Hide();

      if (prevPickedEventHandler != nullptr)
      {
         // Send leave event to picked event handler
         if (prevPickedEventHandler->event_ != nullptr)
         {
            QEvent event(QEvent::Type::Leave);
            prevPickedEventHandler->event_(&event);
         }

         // Reset picked event handler
         weakPickedEventHandler_.reset();
      }
   }
   else if (eventHandler != nullptr)
   {
      // If the event handler changed
      if (prevPickedEventHandler != eventHandler)
      {
         // Send leave event to old event handler
         if (prevPickedEventHandler != nullptr &&
             prevPickedEventHandler->event_ != nullptr)
         {
            QEvent event(QEvent::Type::Leave);
            prevPickedEventHandler->event_(&event);
         }

         // Send enter event to new event handler
         if (eventHandler->event_ != nullptr)
         {
            QEvent event(QEvent::Type::Enter);
            eventHandler->event_(&event);
         }

         // Store picked event handler
         weakPickedEventHandler_ = eventHandler;
      }

      eventHandler.reset();
   }

   if (prevPickedEventHandler != nullptr)
   {
      prevPickedEventHandler.reset();
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
      p->mapChangedOnce_  = true;
      p->mapStylePending_ = false;
      break;

   case QMapLibre::Map::MapChangeDidFinishRenderingFrame:
      if (p->mapStylePending_)
      {
         p->UpdateLoadedStyle();
         p->AddLayers();
         p->mapChangedOnce_  = true;
         p->mapStylePending_ = false;
      }
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
              &manager::RadarProductManager::IncomingLevel2ElevationChanged,
              this,
              [this](std::optional<float> incomingElevation)
              {
                 Q_EMIT widget_->IncomingLevel2ElevationChanged(
                    incomingElevation);
              });
      connect(radarProductManager_.get(),
              &manager::RadarProductManager::Level3ProductsChanged,
              this,
              [this]()
              {
                 const common::Level3ProductCategoryMap& categoryMap =
                    widget_->GetAvailableLevel3Categories();

                 tiltsToIndices_.clear();
                 for (const auto& category : categoryMap)
                 {
                    for (const auto& product : category.second)
                    {
                       for (size_t tiltIndex = 0;
                            tiltIndex < product.second.size();
                            tiltIndex++)
                       {
                          tiltsToIndices_.emplace(product.second[tiltIndex],
                                                  tiltIndex);
                       }
                    }
                 }

                 productAvailabilityUpdated_ = true;
                 CheckLevel3Availability();
                 Q_EMIT widget_->Level3ProductsChanged();
              });

      connect(
         radarProductManager_.get(),
         &manager::RadarProductManager::NewDataAvailable,
         this,
         [this](common::RadarProductGroup             group,
                const std::string&                    product,
                bool                                  isChunks,
                std::chrono::system_clock::time_point latestTime)
         {
            if (autoRefreshEnabled_ &&
                context_->radar_product_group() == group &&
                (group == common::RadarProductGroup::Level2 ||
                 context_->radar_product() == product))
            {
               if (isChunks && autoUpdateEnabled_)
               {
                  // Level 2 products may have multiple time points,
                  // ensure the latest is selected
                  widget_->SelectRadarProduct(group, product);
               }
               else
               {
                  // Create file request
                  const std::shared_ptr<request::NexradFileRequest> request =
                     std::make_shared<request::NexradFileRequest>(
                        radarProductManager_->radar_id());

                  // File request callback
                  if (autoUpdateEnabled_)
                  {
                     connect(
                        request.get(),
                        &request::NexradFileRequest::RequestComplete,
                        this,
                        [group, product, this](
                           const std::shared_ptr<request::NexradFileRequest>&
                              request)
                        {
                           // Select loaded record
                           auto record = request->radar_product_record();

                           // Validate record, and verify current map context
                           // still displays site and product
                           if (record != nullptr &&
                               radarProductManager_ != nullptr &&
                               radarProductManager_->radar_id() ==
                                  request->current_radar_site() &&
                               context_->radar_product_group() == group &&
                               (group == common::RadarProductGroup::Level2 ||
                                context_->radar_product() == product))
                           {
                              if (group == common::RadarProductGroup::Level2)
                              {
                                 // Level 2 products may have multiple time
                                 // points, ensure the latest is selected
                                 widget_->SelectRadarProduct(group, product);
                              }
                              else
                              {
                                 widget_->SelectRadarProduct(record);
                              }

                              // Determine if a screen capture should be
                              // performed
                              auto& generalSettings =
                                 settings::GeneralSettings::Instance();
                              if (generalSettings.screen_capture_on_refresh()
                                     .GetValue())
                              {
                                 widget_->ScreenCapture(
                                    types::CaptureType::SaveImage);
                              }
                           }
                        });
                  }

                  // Load file
                  boost::asio::post(
                     threadPool_,
                     [group, latestTime, request, product, this]()
                     {
                        try
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
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
               }
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
      disconnect(radarProductManager_.get(),
                 &manager::RadarProductManager::IncomingLevel2ElevationChanged,
                 this,
                 nullptr);
   }
}

void MapWidgetImpl::InitializeNewRadarProductView(
   const std::string& colorPalette)
{
   boost::asio::post(threadPool_,
                     [colorPalette, this]()
                     {
                        try
                        {
                           UpdateColorTable(colorPalette);
                           context_->radar_product_view()->Initialize();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
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
      connect(radarProductView.get(),
              &view::RadarProductView::ColorTableLutUpdated,
              widget_,
              static_cast<void (QWidget::*)()>(&QWidget::update),
              Qt::QueuedConnection);
      connect(
         radarProductView.get(),
         &view::RadarProductView::SweepComputed,
         this,
         [=, this]()
         {
            std::shared_ptr<config::RadarSite> radarSite =
               radarProductManager_->radar_site();

            if (map_ != nullptr)
            {
               RadarRangeLayer::Update(
                  map_,
                  radarProductView->range(),
                  {radarSite->latitude(), radarSite->longitude()});
            }

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
                 widget_,
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

void MapWidgetImpl::ScreenCaptureCopy()
{
   const QImage image     = widget_->grabFramebuffer();
   QClipboard*  clipboard = QGuiApplication::clipboard();
   clipboard->setImage(image);

   logger_->info("Map captured to clipboard");
}

void MapWidgetImpl::ScreenCaptureSaveImage()
{
   const QImage image     = widget_->grabFramebuffer();
   const QSize  size      = widget_->size();
   const double latitude  = map_->latitude();
   const double longitude = map_->longitude();
   const double zoom      = map_->zoom();

   std::string                           radarSiteId {"?"};
   std::string                           productName {"?"};
   std::chrono::system_clock::time_point timestamp {};

   auto radarSite = context_->radar_site();
   if (radarSite != nullptr)
   {
      radarSiteId = radarSite->id();
   }

   auto radarProductView = context_->radar_product_view();
   if (radarProductView != nullptr)
   {
      productName = radarProductView->GetRadarProductName();
      timestamp   = radarProductView->selected_time();
   }

   boost::asio::post(
      threadPool_,
      [image,
       size,
       radarSiteId,
       productName,
       timestamp,
       latitude,
       longitude,
       zoom]()
      {
         auto& generalSettings = settings::GeneralSettings::Instance();

         const std::string screenCaptureFolder =
            generalSettings.screen_capture_folder().GetValue();
         const std::string screenCaptureName =
            generalSettings.screen_capture_name().GetValue();

         // Create directory if it doesn't exist
         if (!std::filesystem::exists(screenCaptureFolder))
         {
            std::error_code error;
            if (!std::filesystem::create_directories(screenCaptureFolder,
                                                     error) &&
                error)
            {
               logger_->error(
                  "Unable to create screen capture directory: \"{}\"",
                  screenCaptureFolder);
               return;
            }
         }

         // Format filename
         const std::string screenCaptureFilename = fmt::format(
            fmt::runtime(screenCaptureName),
            fmt::arg("site", radarSiteId),
            fmt::arg("product", productName),
            fmt::arg(
               "timestamp",
               std::chrono::time_point_cast<std::chrono::seconds>(timestamp)),
            fmt::arg("lat", latitude),
            fmt::arg("lon", longitude),
            fmt::arg("zoom", zoom),
            fmt::arg("width", size.width()),
            fmt::arg("height", size.height()));

         // Format path
         const std::string path = fmt::format(
            "{}/{}.png", screenCaptureFolder, screenCaptureFilename);

         // Save image
         if (!image.save(QString::fromStdString(path)))
         {
            logger_->error("Unable to save image: {}", path);
         }
         else
         {
            logger_->info("Map captured to file: {}", path);
         }
      });
}

void MapWidgetImpl::SelectNearestRadarSite(double                     latitude,
                                           double                     longitude,
                                           std::optional<std::string> type)
{
   auto radarSite = config::RadarSite::FindNearest(latitude, longitude, type);

   if (radarSite != nullptr)
   {
      Q_EMIT widget_->RadarSiteRequested(radarSite->id(), false);
   }
}

void MapWidgetImpl::SetRadarSite(const std::string& radarSite,
                                 bool               checkProductAvailability)
{
   // Set the radar site in the context
   context_->set_radar_site(config::RadarSite::Get(radarSite));

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

      // Once the available products are loaded, check to make sure the current
      // one is available
      productAvailabilityCheckNeeded_     = checkProductAvailability;
      productAvailabilityUpdated_         = false;
      productAvailabilityProductSelected_ = false;

      radarProductManager_->UpdateAvailableProducts();
   }
}

void MapWidgetImpl::Update()
{
   QMetaObject::invokeMethod(
      widget_, static_cast<void (QWidget::*)()>(&QWidget::update));

   if (UpdateStoredMapParameters())
   {
      Q_EMIT widget_->MapParametersChanged(
         prevLatitude_, prevLongitude_, prevZoom_, prevBearing_, prevPitch_);
   }
}

void MapWidgetImpl::UpdateColorTable(const std::string& colorPalette)
{
   auto& paletteSetting =
      settings::PaletteSettings::Instance().palette(colorPalette);

   std::string colorTableFile = paletteSetting.GetValue();
   if (colorTableFile.empty())
   {
      colorTableFile = paletteSetting.GetDefault();
   }

   std::unique_ptr<std::istream> colorTableStream =
      util::OpenFile(colorTableFile);
   if (colorTableStream->fail())
   {
      logger_->warn("Could not open color table {}", colorTableFile);
      colorTableStream = util::OpenFile(paletteSetting.GetDefault());
   }

   std::shared_ptr<common::ColorTable> colorTable =
      common::ColorTable::Load(*colorTableStream);
   if (!colorTable->IsValid())
   {
      logger_->warn("Could not load color table {}", colorTableFile);
      colorTableStream = util::OpenFile(paletteSetting.GetDefault());
      colorTable       = common::ColorTable::Load(*colorTableStream);
   }

   context_->radar_product_view()->LoadColorTable(colorTable);
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

void MapWidgetImpl::CheckLevel3Availability()
{
   /*
    * productAvailabilityCheckNeeded_ Only do this when it is indicated that it
    * is needed (mostly on radar site change). This is mainly to avoid potential
    * recursion with SelectRadarProduct calls.
    *
    * productAvailabilityUpdated_ Only update once the product availability
    * has been updated
    *
    * productAvailabilityProductSelected_ Only update once the radar site is
    * fully selected
    */
   if (!(productAvailabilityCheckNeeded_ && productAvailabilityUpdated_ &&
         productAvailabilityProductSelected_))
   {
      return;
   }
   productAvailabilityCheckNeeded_ = false;

   // Get radar product view for fallback and level2 selection
   auto radarProductView = context_->radar_product_view();
   if (radarProductView == nullptr)
   {
      return;
   }

   // Only do this for level3 products
   if (widget_->GetRadarProductGroup() != common::RadarProductGroup::Level3)
   {
      widget_->SelectRadarProduct(radarProductView->GetRadarProductGroup(),
                                  radarProductView->GetRadarProductName(),
                                  0,
                                  radarProductView->selected_time(),
                                  false);
      return;
   }

   const common::Level3ProductCategoryMap& categoryMap =
      widget_->GetAvailableLevel3Categories();

   const std::string& productTilt = context_->radar_product();
   const std::string& productName =
      common::GetLevel3ProductByAwipsId(productTilt);
   const common::Level3ProductCategory productCategory =
      common::GetLevel3CategoryByProduct(productName);
   if (productCategory == common::Level3ProductCategory::Unknown)
   {
      // Default to the same as already selected
      widget_->SelectRadarProduct(radarProductView->GetRadarProductGroup(),
                                  radarProductView->GetRadarProductName(),
                                  0,
                                  radarProductView->selected_time(),
                                  false);
      return;
   }

   const auto& availableProductsIt = categoryMap.find(productCategory);
   // Has no products in this category, do not change categories
   if (availableProductsIt == categoryMap.cend())
   {
      // Default to the same as already selected
      widget_->SelectRadarProduct(radarProductView->GetRadarProductGroup(),
                                  radarProductView->GetRadarProductName(),
                                  0,
                                  radarProductView->selected_time(),
                                  false);
      return;
   }

   const auto& availableProducts = availableProductsIt->second;
   const auto& availableTiltsIt  = availableProducts.find(productName);

   const auto& availableTilts =
      availableTiltsIt == availableProducts.cend() ?
         // Does not have the same product, but has others in the same category.
         // Switch to the default product and tilt in this category.
         availableProducts.at(common::GetLevel3ProductByAwipsId(
            common::GetLevel3CategoryDefaultProduct(productCategory,
                                                    categoryMap))) :
         // Has the same product
         availableTiltsIt->second;

   // Try to match the tilt to the last tilt.
   if (currentTiltIndex_ < availableTilts.size())
   {
      widget_->SelectRadarProduct(common::RadarProductGroup::Level3,
                                  availableTilts[currentTiltIndex_],
                                  0,
                                  widget_->GetSelectedTime());
   }
   else if (availableTilts.size() > 0)
   {
      widget_->SelectRadarProduct(common::RadarProductGroup::Level3,
                                  availableTilts[availableTilts.size() - 1],
                                  0,
                                  widget_->GetSelectedTime());
   }
   else
   {
      // No tilts available in this case, default to the same as already
      // selected
      widget_->SelectRadarProduct(radarProductView->GetRadarProductGroup(),
                                  radarProductView->GetRadarProductName(),
                                  0,
                                  radarProductView->selected_time(),
                                  false);
   }
}

} // namespace scwx::qt::map

#include "map_widget.moc"
