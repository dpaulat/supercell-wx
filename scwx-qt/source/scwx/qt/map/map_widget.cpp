#include <scwx/qt/map/map_annotation_layer.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
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
#include <scwx/qt/types/layer_types.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/qt/ui/edit_marker_dialog.hpp>
#include <scwx/qt/util/file.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/view/radar_product_view_factory.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>
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
#include <QCursor>
#include <QContextMenuEvent>
#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QPinchGesture>
#include <QPixmap>
#include <QString>
#include <QStyleHints>
#include <QTextDocument>
#include <QTimer>

namespace scwx::qt::map
{

// Cursor artwork and transient Qt-owned labels use tuned values and normal Qt
// parent ownership patterns.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-owning-memory)
namespace
{

constexpr int kFallbackEraseCursorRadiusPx {8};

std::string CustomLayerId(types::LayerType        type,
                          types::LayerDescription description)
{
   if (type == types::LayerType::Alert &&
       std::holds_alternative<awips::Phenomenon>(description))
   {
      return fmt::format(
         "alert.{}",
         awips::GetPhenomenonCode(std::get<awips::Phenomenon>(description)));
   }

   return types::GetLayerName(type, description);
}

float LayerOpacity(const types::LayerInfo& info)
{
   return types::LayerSupportsOpacity(info.type_) ? info.opacity_ : 1.0f;
}

/** Ring + eraser in pixmap so KDE/Wayland compositor tracks cursor with zero
 * lag. Pixmap radius is capped (~124px) for display only; geographic erase pick
 * and `EraseCursorRadiusPx` use the full brush width in ground meters. */
QCursor CreateEraseCursor(int radiusPx)
{
   constexpr int kPad      = 8;
   constexpr int kMaxSize  = 256;
   const int     maxRadius = (kMaxSize - kPad) / 2;
   const int     r         = std::clamp(radiusPx, 4, maxRadius);
   const int     size      = std::clamp(r * 2 + kPad, 32, kMaxSize);
   const qreal   center    = static_cast<qreal>(size) / 2.0;

   QPixmap pixmap {size, size};
   pixmap.fill(Qt::transparent);

   QPainter painter {&pixmap};
   painter.setRenderHint(QPainter::Antialiasing, true);
   painter.setBrush(Qt::NoBrush);

   const QPointF ringCenter {center, center};
   const auto    ringRadius = static_cast<qreal>(r);

   // Dark halo — readable on bright radar returns.
   QPen haloPen {QColor {0, 0, 0, 210}};
   haloPen.setWidthF(3.0);
   haloPen.setCapStyle(Qt::RoundCap);
   painter.setPen(haloPen);
   painter.drawEllipse(ringCenter, ringRadius, ringRadius);

   // Light dashed ring — readable on dark map / satellite.
   QPen dashPen {QColor {255, 255, 255, 245}};
   dashPen.setWidthF(1.75);
   dashPen.setStyle(Qt::DashLine);
   dashPen.setDashPattern({5.0, 4.0});
   dashPen.setCapStyle(Qt::RoundCap);
   painter.setPen(dashPen);
   painter.drawEllipse(ringCenter, ringRadius, ringRadius);

   painter.translate(center, center);
   painter.rotate(-35.0);
   painter.setPen(QPen {QColor {36, 36, 36}, 1.0});
   painter.setBrush(QColor {255, 186, 104});
   painter.drawRoundedRect(QRectF {-5.0, -7.0, 10.0, 7.0}, 2.0, 2.0);
   painter.setBrush(QColor {239, 83, 80});
   painter.drawRoundedRect(QRectF {-5.0, 0.0, 10.0, 5.0}, 1.5, 1.5);
   painter.setBrush(QColor {245, 245, 245});
   painter.drawRect(QRectF {-4.0, 4.0, 8.0, 2.5});

   return QCursor {pixmap, static_cast<int>(center), static_cast<int>(center)};
}

QString FormatMeasurementDistance(double meters)
{
   const auto units = types::GetDistanceUnitsFromName(
      settings::UnitSettings::Instance().distance_units().GetValue());
   const double display = meters * scwx::common::kKilometersPerMeter *
                          types::GetDistanceUnitsScale(units);
   std::string abbrev = types::GetDistanceUnitsAbbreviation(units);
   if (abbrev.empty())
   {
      abbrev = "user";
   }

   int decimals = 1;
   if (display < 1.0)
   {
      decimals = 2;
   }
   else if (display >= 10.0)
   {
      decimals = 0;
   }

   return QStringLiteral("%1 %2")
      .arg(QString::number(display, 'f', decimals))
      .arg(QString::fromStdString(abbrev));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-owning-memory)
} // namespace

static const std::string logPrefix_ = "scwx::qt::map::map_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr double kDefaultZoom_ {7.0};
static constexpr int    kMapPaneContextMenuDebounceMs {200};
static constexpr double kDoubleClickZoomIn {2.0};
static constexpr double kDoubleClickZoomOut {0.5};

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
       annotationLayer_ {nullptr},
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

      // Map data: default radar site is set from MapWidget ctor after
      // std::make_unique finishes so MapWidget::p is valid (avoid callbacks
      // that use the outer widget during p's initialization).
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
      if (eraseCursorActive_ && QApplication::overrideCursor() != nullptr)
      {
         QApplication::restoreOverrideCursor();
      }

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
   void UpdateLayerOpacities();
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
   void SelectNearestRadarSite(double                          latitude,
                               double                          longitude,
                               std::optional<types::RadarType> type);
   void SetRadarSite(const std::string& radarSite,
                     bool               checkProductAvailability = false);
   [[nodiscard]] QPointF EraseCursorWidgetPosition() const;
   [[nodiscard]] int     EraseCursorRadiusPx(const QPointF& widgetPos) const;
   void                  UpdateAnnotationCursor();
   void                  UpdateMeasureLabels();
   void                  UpdateColorTable(const std::string& colorPalette);
   void                  UpdateColorTable(
                       const std::string&                             colorPalette,
                       const std::shared_ptr<view::RadarProductView>& radarProductView);
   void UpdateLoadedStyle();
   bool UpdateStoredMapParameters();
   void CheckLevel3Availability();

   void SyncStoredViewFromMap();
   void RequestRepaint();
   void CancelPaneContextMenuDebounce();
   void GetMapViewParameters(double& latitude,
                             double& longitude,
                             double& zoom,
                             double& bearing,
                             double& pitch) const;

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
   std::unordered_map<std::string, std::shared_ptr<GenericLayer>>
      genericLayerMap_ {};

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

   std::shared_ptr<RadarProductLayer>         radarProductLayer_;
   std::shared_ptr<OverlayLayer>              overlayLayer_;
   std::shared_ptr<OverlayProductLayer>       overlayProductLayer_ {nullptr};
   std::shared_ptr<PlacefileLayer>            placefileLayer_;
   std::shared_ptr<MarkerLayer>               markerLayer_;
   std::shared_ptr<ColorTableLayer>           colorTableLayer_;
   std::shared_ptr<RadarSiteLayer>            radarSiteLayer_ {nullptr};
   std::shared_ptr<MapAnnotationLayer>        annotationLayer_;
   std::unordered_map<std::uint64_t, QLabel*> measureLabels_ {};

   std::list<std::shared_ptr<PlacefileLayer>> placefileLayers_ {};

   bool autoRefreshEnabled_;
   bool autoUpdateEnabled_;
   bool smoothingEnabled_ {false};

   common::Level2Product selectedLevel2Product_;

   bool hasMouse_ {false};
   bool eraseCursorActive_ {false};
   int  eraseCursorRadiusPx_ {-1};
   bool isPainting_ {false};
   bool lastItemPicked_ {false};

   // Right-button context menu: arm on press, show on release (debounced) if
   // drag never exceeded startDragDistance. Debounce canceled on any new press;
   // double-right and left+right chord skip menu. dragThresholdSq_ cached at
   // press time so startDragDistance() is not queried on every move event.
   bool     paneContextMenuArmed_ {false};
   bool     paneContextMenuDragTooFar_ {false};
   QPointF  paneContextMenuPressPos_ {};
   qreal    paneContextMenuDragThresholdSq_ {0};
   uint64_t paneContextMenuDebounce_ {0};
   bool     suppressContextMenuOnNextRightRelease_ {false};

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
   p->SetRadarSite(
      settings::GeneralSettings::Instance().default_radar_site().GetValue());

   if (settings::GeneralSettings::Instance().anti_aliasing_enabled().GetValue())
   {
      QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
      surfaceFormat.setSamples(4);
      setFormat(surfaceFormat);
   }

   setFocusPolicy(Qt::StrongFocus);
   setMouseTracking(true);

   // Avoid Qt dispatching a context menu during the right-button press; that
   // would run a blocking QMenu in MainWindow and steal the right-drag
   // sequence (e.g. rotate / prior pan-on-right workflows).
   setContextMenuPolicy(Qt::NoContextMenu);

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
              static const int opacityColumn =
                 static_cast<int>(model::LayerModel::Column::Opacity);
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
              else if (topLeft.column() <= opacityColumn &&
                       opacityColumn <= bottomRight.column())
              {
                 UpdateLayerOpacities();
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
      if (hasMouse_ && map_ != nullptr)
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
      if (hasMouse_ && map_ != nullptr)
      {
         QClipboard* clipboard  = QGuiApplication::clipboard();
         auto        coordinate = map_->coordinateForPixel(lastPos_);
         std::string text =
            fmt::format("{}, {}", coordinate.first, coordinate.second);
         clipboard->setText(QString::fromStdString(text));
      }
      break;

   case types::Hotkey::CopyMapCoordinates:
      if (context_->settings().isActive_ && map_ != nullptr)
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

   if (map_ == nullptr)
   {
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
   if (map_ == nullptr)
   {
      return;
   }

   if (gesture->changeFlags() & QPinchGesture::ChangeFlag::ScaleFactorChanged)
   {
#if defined(__APPLE__)
      // The macOS native pinch recognizer stores centerPoint() in
      // widget-local coordinates.
      map_->scaleBy(gesture->scaleFactor(), gesture->centerPoint());
#else
      // The generic pinch recognizer stores centerPoint() in global
      // coordinates, so convert it to widget-local coordinates first.
      map_->scaleBy(gesture->scaleFactor(),
                    widget_->mapFromGlobal(gesture->centerPoint()));
#endif
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
   return p->radarProductManager_ != nullptr ?
             p->radarProductManager_->incoming_level_2_elevation() :
             std::optional<float> {};
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
   return p->radarProductManager_ != nullptr ?
             p->radarProductManager_->default_time_zone() :
             scwx::util::time::current_time_zone();
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

   if (p->radarProductManager_ == nullptr)
   {
      p->context_->set_radar_product_group(group);
      p->context_->set_radar_product(productName);
      p->context_->set_radar_product_code(productCode);
      p->RadarProductViewDisconnect();
      p->context_->set_radar_product_view(nullptr);
      return;
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
   const std::shared_ptr<config::RadarSite> currentRadarSite = GetRadarSite();
   auto radarProductView = p->context_->radar_product_view();

   if (radarSite == nullptr)
   {
      if (currentRadarSite == nullptr)
      {
         return;
      }

      if (p->autoRefreshEnabled_ && p->radarProductManager_ != nullptr &&
          radarProductView != nullptr)
      {
         p->radarProductManager_->EnableRefresh(
            radarProductView->GetRadarProductGroup(),
            radarProductView->GetRadarProductName(),
            false,
            p->uuid_);
      }

      p->RadarProductViewDisconnect();
      p->context_->set_radar_product_view(nullptr);
      p->SetRadarSite("");
      p->AddLayers();
      p->Update();

      Q_EMIT RadarSiteUpdated(nullptr);
      return;
   }

   // Verify radar site has changed
   if (currentRadarSite == nullptr || radarSite->id() != currentRadarSite->id())
   {
      // setCoordinate: map exists only after initializeGL/ResetMap; first fit
      // is applied from radarProductManager_ in ResetMap.
      if (updateCoordinates && p->map_ != nullptr)
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
      else if (p->context_->radar_product_group() !=
               common::RadarProductGroup::Unknown)
      {
         SelectRadarProduct(p->context_->radar_product_group(),
                            p->context_->radar_product(),
                            p->context_->radar_product_code());
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
   p->UpdateAnnotationCursor();
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
         std::optional<types::RadarType> type = std::nullopt;

         if (generalSettings.auto_navigate_to_wsr88d_only().GetValue())
         {
            // Find the nearest WSR-88D radar
            type = types::RadarType::WSR88D;
         }

         // Find the nearest radar
         const bool isArchiveMode =
            manager::TimelineManager::Instance()->GetViewType() ==
            types::MapTime::Archive;
         const bool includeDown           = isArchiveMode;
         const bool includeDecommissioned = isArchiveMode;
         const std::shared_ptr<config::RadarSite> nearestRadarSite =
            config::RadarSite::FindNearest(
               latitude, longitude, type, includeDown, includeDecommissioned);

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

      // Match stored view to the engine so the next Update() does not emit
      // MapParametersChanged again (avoids feedback loops between panes).
      p->SyncStoredViewFromMap();

      p->RequestRepaint();
   }
}

void MapWidgetImpl::SyncStoredViewFromMap()
{
   if (map_ == nullptr)
   {
      return;
   }
   prevLatitude_  = map_->latitude();
   prevLongitude_ = map_->longitude();
   prevZoom_      = map_->zoom();
   prevBearing_   = map_->bearing();
   prevPitch_     = map_->pitch();
}

void MapWidgetImpl::RequestRepaint()
{
   QMetaObject::invokeMethod(
      widget_, static_cast<void (QWidget::*)()>(&QWidget::update));
}

void MapWidgetImpl::CancelPaneContextMenuDebounce()
{
   ++paneContextMenuDebounce_;
}

void MapWidgetImpl::GetMapViewParameters(double& latitude,
                                         double& longitude,
                                         double& zoom,
                                         double& bearing,
                                         double& pitch) const
{
   if (map_ != nullptr)
   {
      latitude  = map_->latitude();
      longitude = map_->longitude();
      zoom      = map_->zoom();
      bearing   = map_->bearing();
      pitch     = map_->pitch();
   }
   else
   {
      latitude  = prevLatitude_;
      longitude = prevLongitude_;
      zoom      = prevZoom_;
      bearing   = prevBearing_;
      pitch     = prevPitch_;
   }
}

void MapWidget::GetMapViewParameters(double& latitude,
                                     double& longitude,
                                     double& zoom,
                                     double& bearing,
                                     double& pitch) const
{
   p->GetMapViewParameters(latitude, longitude, zoom, bearing, pitch);
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
   if (p->map_ == nullptr)
   {
      logger_->info("Layers: (map not initialized)");
      return;
   }
   logger_->info("Layers: {}", p->map_->layerIds().join(", ").toStdString());
}

void MapWidgetImpl::AddLayers()
{
   if (styleLayers_.isEmpty() || map_ == nullptr)
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
   genericLayerMap_.clear();
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

   if (annotationLayer_ == nullptr)
   {
      annotationLayer_ = std::make_shared<MapAnnotationLayer>(glContext_);
      QObject::connect(annotationLayer_.get(),
                       &MapAnnotationLayer::ToolChanged,
                       widget_,
                       [this](MapAnnotationTool /*tool*/)
                       { UpdateAnnotationCursor(); });
   }
   AddLayer("scwx.map.annotations", annotationLayer_, "");
   UpdateAnnotationCursor();

   Q_EMIT widget_->MapAnnotationLayerReady();

   // Color table layer is omitted when there is no radar product view, but
   // map context can still hold bottom margin from a previous site; clear it.
   static const std::string kColorTableLayerId = types::GetLayerName(
      types::LayerType::Information, types::InformationLayer::ColorTable);
   if (std::ranges::find(layerList_, kColorTableLayerId) == layerList_.cend())
   {
      context_->set_color_table_margins({});
   }

   UpdateLayerOpacities();
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
         connect(radarSiteLayer_.get(),
                 &RadarSiteLayer::RadarSiteSelected,
                 this,
                 [this](const std::string& id)
                 {
                    auto& generalSettings =
                       settings::GeneralSettings::Instance();
                    auto selectedRadarSite = widget_->GetRadarSite();
                    const std::string requestedRadarSite =
                       (selectedRadarSite != nullptr &&
                        selectedRadarSite->id() == id) ?
                          std::string {} :
                          id;
                    widget_->RadarSiteRequested(
                       requestedRadarSite,
                       generalSettings.center_on_radar_selection().GetValue());
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
         if (radarProductView != nullptr && radarProductManager_ != nullptr &&
             radarProductManager_->radar_site() != nullptr)
         {
            std::shared_ptr<config::RadarSite> radarSite =
               radarProductManager_->radar_site();
            RadarRangeLayer::Add(
               map_,
               radarProductView->range(),
               {radarSite->latitude(), radarSite->longitude()},
               QString::fromStdString(before),
               LayerOpacity(layerModel_->GetLayerInfo(type, description)));
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
      genericLayerMap_[id] = layer;

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

void MapWidgetImpl::UpdateLayerOpacities()
{
   const types::LayerVector layers = layerModel_->GetLayers();
   for (const auto& info : layers)
   {
      const float opacity = LayerOpacity(info);

      if (info.type_ == types::LayerType::Data &&
          std::holds_alternative<types::DataLayer>(info.description_) &&
          std::get<types::DataLayer>(info.description_) ==
             types::DataLayer::RadarRange)
      {
         RadarRangeLayer::SetOpacity(map_, opacity);
         continue;
      }

      const std::string id = CustomLayerId(info.type_, info.description_);
      auto              it = genericLayerMap_.find(id);
      if (it != genericLayerMap_.end() && it->second != nullptr)
      {
         it->second->set_opacity(opacity);
      }
   }

   widget_->update();
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
   p->UpdateAnnotationCursor();
}

void MapWidget::leaveEvent(QEvent* /* ev */)
{
   p->hasMouse_ = false;
   p->UpdateAnnotationCursor();
}

void MapWidget::contextMenuEvent(QContextMenuEvent* event)
{
   Q_EMIT MapPaneContextMenuRequested(event->globalPos());
   event->accept();
}

void MapWidget::keyPressEvent(QKeyEvent* ev)
{
   if (p->hotkeyManager_->HandleKeyPress(ev))
   {
      ev->accept();
      return;
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

   if (p->map_ == nullptr)
   {
      ev->accept();
      return;
   }

   if (ev->type() == QEvent::Type::MouseButtonPress &&
       ev->button() == Qt::MouseButton::LeftButton &&
       p->annotationLayer_ != nullptr &&
       (ev->modifiers() & Qt::KeyboardModifier::ControlModifier) == 0 &&
       p->annotationLayer_->tool() != MapAnnotationTool::None)
   {
      p->annotationLayer_->HandleMousePress(p->map_, ev->position());
      ev->accept();
      return;
   }

   p->CancelPaneContextMenuDebounce();
   if (ev->type() == QEvent::Type::MouseButtonPress)
   {
      if (ev->buttons() ==
          (Qt::MouseButton::LeftButton | Qt::MouseButton::RightButton))
      {
         changeStyle();
         // Disarm and mark too-far so rotation is immediately permitted if the
         // user continues dragging with both buttons held then releases left.
         p->paneContextMenuArmed_      = false;
         p->paneContextMenuDragTooFar_ = true;
      }
      else if (ev->buttons() == Qt::MouseButton::MiddleButton)
      {
         auto& generalSettings = settings::GeneralSettings::Instance();

         // Select nearest radar on middle click
         std::optional<types::RadarType> type = std::nullopt;

         if (generalSettings.auto_navigate_to_wsr88d_only().GetValue())
         {
            // Select nearest WSR-88D radar on middle click
            type = types::RadarType::WSR88D;
         }

         auto coordinate = p->map_->coordinateForPixel(p->lastPos_);
         p->SelectNearestRadarSite(coordinate.first, coordinate.second, type);
      }
      else if (ev->button() == Qt::MouseButton::RightButton)
      {
         p->paneContextMenuArmed_      = true;
         p->paneContextMenuDragTooFar_ = false;
         p->paneContextMenuPressPos_   = ev->position();
         const auto d =
            static_cast<qreal>(QApplication::styleHints()->startDragDistance());
         p->paneContextMenuDragThresholdSq_ = d * d;
         p->suppressContextMenuOnNextRightRelease_ =
            static_cast<bool>(ev->flags() & Qt::MouseEventCreatedDoubleClick);
      }
   }

   ev->accept();
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
   p->lastPos_       = ev->position();
   p->lastGlobalPos_ = ev->globalPosition();

   if (p->map_ == nullptr)
   {
      ev->accept();
      return;
   }

   p->CancelPaneContextMenuDebounce();

   if (ev->button() == Qt::MouseButton::LeftButton)
   {
      p->map_->scaleBy(kDoubleClickZoomIn, p->lastPos_);
   }
   else if (ev->button() == Qt::MouseButton::RightButton)
   {
      p->map_->scaleBy(kDoubleClickZoomOut, p->lastPos_);
      p->suppressContextMenuOnNextRightRelease_ = true;
   }

   p->UpdateAnnotationCursor();
   ev->accept();
}

void MapWidget::mouseMoveEvent(QMouseEvent* ev)
{
   if (p->map_ == nullptr)
   {
      p->lastPos_       = ev->position();
      p->lastGlobalPos_ = ev->globalPosition();
      ev->accept();
      return;
   }

   // Ctrl + left-drag: cancel any in-progress annotation interaction and pan
   // the map instead of drawing.
   if (ev->buttons() == Qt::MouseButton::LeftButton &&
       (ev->modifiers() & Qt::KeyboardModifier::ControlModifier) != 0)
   {
      if (p->annotationLayer_ != nullptr)
      {
         p->annotationLayer_->CancelInteraction();
      }
      const QPointF delta = ev->position() - p->lastPos_;
      if (!delta.isNull())
      {
         p->map_->moveBy(delta);
      }
      p->lastPos_       = ev->position();
      p->lastGlobalPos_ = ev->globalPosition();
      p->UpdateAnnotationCursor();
      ev->accept();
      return;
   }

   if (ev->buttons() == Qt::MouseButton::LeftButton &&
       p->annotationLayer_ != nullptr &&
       p->annotationLayer_->IsActivelyDrawing() &&
       (ev->modifiers() & Qt::KeyboardModifier::ControlModifier) == 0)
   {
      p->annotationLayer_->HandleMouseMove(p->map_, ev->position());
      p->lastPos_       = ev->position();
      p->lastGlobalPos_ = ev->globalPosition();
      p->UpdateAnnotationCursor();
      ev->accept();
      return;
   }

   if (p->paneContextMenuArmed_ &&
       (ev->buttons() & Qt::MouseButton::RightButton) &&
       !p->paneContextMenuDragTooFar_)
   {
      const QPointF d = ev->position() - p->paneContextMenuPressPos_;
      if (d.x() * d.x() + d.y() * d.y() > p->paneContextMenuDragThresholdSq_)
      {
         p->paneContextMenuDragTooFar_ = true;
         // Reset lastPos_ to current so the first rotateBy call below uses only
         // the incremental delta from this tick, not the full suppressed
         // distance.
         p->lastPos_ = ev->position();
      }
   }

   const QPointF delta = ev->position() - p->lastPos_;

   if (!delta.isNull())
   {
      if (ev->buttons() == Qt::MouseButton::LeftButton)
      {
         p->map_->moveBy(delta);
      }
      else if (ev->buttons() == Qt::MouseButton::RightButton &&
               p->paneContextMenuDragTooFar_)
      {
         p->map_->rotateBy(p->lastPos_, ev->position());
      }
   }

   p->lastPos_       = ev->position();
   p->lastGlobalPos_ = ev->globalPosition();
   p->UpdateAnnotationCursor();
   ev->accept();
}

void MapWidget::mouseReleaseEvent(QMouseEvent* ev)
{
   p->lastPos_       = ev->position();
   p->lastGlobalPos_ = ev->globalPosition();

   // Let annotation tools see left-button release when a tool is active.
   if (ev->button() == Qt::MouseButton::LeftButton &&
       p->annotationLayer_ != nullptr && p->map_ != nullptr &&
       p->annotationLayer_->tool() != MapAnnotationTool::None)
   {
      p->annotationLayer_->HandleMouseRelease(p->map_, ev->position());
   }

   if (ev->button() == Qt::MouseButton::RightButton)
   {
      if (p->suppressContextMenuOnNextRightRelease_)
      {
         p->suppressContextMenuOnNextRightRelease_ = false;
      }
      else if (p->paneContextMenuArmed_ && !p->paneContextMenuDragTooFar_)
      {
         const uint64_t token = p->paneContextMenuDebounce_;
         const QPoint   gpos  = ev->globalPosition().toPoint();
         QTimer::singleShot(kMapPaneContextMenuDebounceMs,
                            this,
                            [this, token, gpos]()
                            {
                               if (p->paneContextMenuDebounce_ != token)
                               {
                                  return;
                               }
                               Q_EMIT MapPaneContextMenuRequested(gpos);
                            });
      }
      p->paneContextMenuArmed_      = false;
      p->paneContextMenuDragTooFar_ = false;
   }

   ev->accept();
}

std::shared_ptr<MapAnnotationLayer> MapWidget::map_annotation_layer() const
{
   return p->annotationLayer_;
}

void MapWidget::SyncEraseCursor()
{
   p->UpdateAnnotationCursor();
}

void MapWidget::resizeEvent(QResizeEvent* event)
{
   QOpenGLWidget::resizeEvent(event);
   p->UpdateAnnotationCursor();
}

QPointF MapWidgetImpl::EraseCursorWidgetPosition() const
{
   const QPointF widgetPos = widget_->mapFromGlobal(QCursor::pos());
   if (QRectF {widget_->rect()}.contains(widgetPos))
   {
      return widgetPos;
   }
   return lastPos_;
}

int MapWidgetImpl::EraseCursorRadiusPx(const QPointF& widgetPos) const
{
   if (map_ == nullptr || annotationLayer_ == nullptr)
   {
      return kFallbackEraseCursorRadiusPx;
   }

   // Brush size is ground diameter; ring radius is half that, in screen pixels.
   const double radiusM = annotationLayer_->style().strokeWidthM.value() * 0.5;
   const double mpp = util::maplibre::MetersPerPixelAt(map_, widgetPos).value();
   if (mpp <= 0.0)
   {
      return kFallbackEraseCursorRadiusPx;
   }

   const int radiusPx = static_cast<int>(std::round(radiusM / mpp));
   // Cap ring size on tiny panes; erase pick still uses full `strokeWidthM`.
   const int maxPx =
      static_cast<int>(0.5 * std::min(widget_->width(), widget_->height()));
   return std::clamp(radiusPx, 2, maxPx);
}

void MapWidgetImpl::UpdateAnnotationCursor()
{
   const bool showErase =
      context_->settings().isActive_ && annotationLayer_ != nullptr &&
      annotationLayer_->tool() == MapAnnotationTool::Erase &&
      (hasMouse_ || widget_->underMouse());

   if (showErase)
   {
      constexpr int kRadiusRebuildThresholdPx {2};

      const int  radiusPx = EraseCursorRadiusPx(EraseCursorWidgetPosition());
      const bool radiusChanged =
         !eraseCursorActive_ ||
         std::abs(radiusPx - eraseCursorRadiusPx_) >= kRadiusRebuildThresholdPx;

      if (!eraseCursorActive_ || radiusChanged)
      {
         const QCursor eraseCursor = CreateEraseCursor(radiusPx);
         if (QApplication::overrideCursor() == nullptr)
         {
            QApplication::setOverrideCursor(eraseCursor);
         }
         else
         {
            QApplication::changeOverrideCursor(eraseCursor);
         }
         eraseCursorActive_   = true;
         eraseCursorRadiusPx_ = radiusPx;
      }
      return;
   }

   if (eraseCursorActive_)
   {
      if (QApplication::overrideCursor() != nullptr)
      {
         QApplication::restoreOverrideCursor();
      }
      eraseCursorActive_   = false;
      eraseCursorRadiusPx_ = -1;
   }
}

void MapWidgetImpl::UpdateMeasureLabels()
{
   if (annotationLayer_ == nullptr || map_ == nullptr)
   {
      for (auto& [id, label] : measureLabels_)
      {
         static_cast<void>(id);
         if (label != nullptr)
         {
            label->hide();
            label->deleteLater();
         }
      }
      measureLabels_.clear();
      return;
   }

   const auto overlays = annotationLayer_->GetMeasurementOverlays();
   std::unordered_set<std::uint64_t> activeIds;
   activeIds.reserve(overlays.size());

   for (const auto& overlay : overlays)
   {
      activeIds.insert(overlay.id);

      QLabel*& label = measureLabels_[overlay.id];
      if (label == nullptr)
      {
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         label = new QLabel(widget_);
         label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
         label->setStyleSheet(
            QStringLiteral("background-color: rgba(32, 37, 43, 220);"
                           "color: white;"
                           "border: 1px solid rgba(255,255,255,48);"
                           "border-radius: 6px;"
                           "padding: 2px 6px;"));
      }

      const QString labelText =
         FormatMeasurementDistance(overlay.distanceM.value());
      if (label->text() != labelText)
      {
         label->setText(labelText);
         label->adjustSize();
      }

      const QPointF anchorPoint = map_->pixelForCoordinate(
         {overlay.labelAnchor.latitude_, overlay.labelAnchor.longitude_});
      const int x =
         static_cast<int>(std::round(anchorPoint.x())) - label->width() / 2;
      const int y =
         static_cast<int>(std::round(anchorPoint.y())) - label->height() - 14;

      if (x + label->width() < 0 || y + label->height() < 0 ||
          x > widget_->width() || y > widget_->height())
      {
         label->hide();
         continue;
      }

      label->move(x, y);
      label->show();
      label->raise();
   }

   for (auto it = measureLabels_.begin(); it != measureLabels_.end();)
   {
      if (activeIds.contains(it->first))
      {
         ++it;
         continue;
      }

      if (it->second != nullptr)
      {
         it->second->hide();
         it->second->deleteLater();
      }
      it = measureLabels_.erase(it);
   }
}

void MapWidget::wheelEvent(QWheelEvent* ev)
{
   if (p->map_ == nullptr)
   {
      return;
   }

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
   p->UpdateAnnotationCursor();

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
         radarProductManager_ != nullptr ? radarProductManager_->radar_site() :
                                           nullptr;
      if (radarSite != nullptr)
      {
         map_->setCoordinateZoom(
            {radarSite->latitude(), radarSite->longitude()}, prevZoom_);
      }
      else
      {
         map_->setCoordinateZoom({prevLatitude_, prevLongitude_}, prevZoom_);
      }
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
                            [&](const auto& mapStyle) {
                               return mapStyle.name_ == preferredStyleName;
                            }) != mapProviderInfo.mapStyles_.cend())
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
   p->UpdateMeasureLabels();

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
              [this](std::optional<float> incomingElevation) {
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
   auto radarProductView = context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      boost::asio::post(threadPool_,
                        [colorPalette, radarProductView, this]()
                        {
                           if (radarProductView !=
                               context_->radar_product_view())
                           {
                              // If the radar product view has changed, don't
                              // initialize
                              return;
                           }

                           try
                           {
                              UpdateColorTable(colorPalette, radarProductView);
                              radarProductView->Initialize();
                           }
                           catch (const std::exception& ex)
                           {
                              logger_->error(ex.what());
                           }
                        });
   }

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
               radarProductManager_ != nullptr ?
                  radarProductManager_->radar_site() :
                  nullptr;

            if (map_ != nullptr && radarSite != nullptr)
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

void MapWidgetImpl::SelectNearestRadarSite(double latitude,
                                           double longitude,
                                           std::optional<types::RadarType> type)
{
   const bool isArchiveMode =
      manager::TimelineManager::Instance()->GetViewType() ==
      types::MapTime::Archive;
   const bool includeDown           = isArchiveMode;
   const bool includeDecommissioned = isArchiveMode;
   const auto radarSite             = config::RadarSite::FindNearest(
      latitude, longitude, type, includeDown, includeDecommissioned);

   if (radarSite != nullptr)
   {
      Q_EMIT widget_->RadarSiteRequested(radarSite->id(), false);
   }
}

void MapWidgetImpl::SetRadarSite(const std::string& radarSite,
                                 bool               checkProductAvailability)
{
   // Set the radar site in the context
   const std::string canonicalRadarSite =
      common::GetCanonicalRadarId(radarSite);
   const auto newRadarSite = config::RadarSite::Get(canonicalRadarSite);
   context_->set_radar_site(newRadarSite);

   const std::shared_ptr<config::RadarSite> currentRadarSite =
      radarProductManager_ != nullptr ? radarProductManager_->radar_site() :
                                        nullptr;

   // Check if radar site has changed
   if ((currentRadarSite == nullptr && !canonicalRadarSite.empty()) ||
       (currentRadarSite != nullptr &&
        canonicalRadarSite != currentRadarSite->id()))
   {
      // Disconnect signals from old RadarProductManager
      RadarProductManagerDisconnect();

      // Update views
      if (canonicalRadarSite.empty())
      {
         radarProductManager_.reset();
         context_->overlay_product_view()->set_radar_product_manager(nullptr);
         productAvailabilityCheckNeeded_     = false;
         productAvailabilityUpdated_         = false;
         productAvailabilityProductSelected_ = false;
         return;
      }

      // Set new RadarProductManager
      radarProductManager_ =
         manager::RadarProductManager::Instance(canonicalRadarSite);
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

   if (map_ == nullptr)
   {
      return;
   }

   if (UpdateStoredMapParameters())
   {
      Q_EMIT widget_->MapParametersChanged(
         prevLatitude_, prevLongitude_, prevZoom_, prevBearing_, prevPitch_);
   }
}

void MapWidgetImpl::UpdateColorTable(const std::string& colorPalette)
{
   UpdateColorTable(colorPalette, context_->radar_product_view());
}

void MapWidgetImpl::UpdateColorTable(
   const std::string&                             colorPalette,
   const std::shared_ptr<view::RadarProductView>& radarProductView)
{
   if (radarProductView == nullptr)
   {
      return;
   }

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

   radarProductView->LoadColorTable(colorTable);
}

bool MapWidgetImpl::UpdateStoredMapParameters()
{
   if (map_ == nullptr)
   {
      return false;
   }

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
