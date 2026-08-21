#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/map/map_link_policy.hpp>
#include <scwx/qt/map/map_pane_splitter_state.hpp>
#include <scwx/qt/map/map_pane_view_link_state.hpp>
#include <scwx/qt/map/map_popout_frame.hpp>

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/main/application.hpp>
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/manager/alert_manager.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/update_manager.hpp>
#include <scwx/qt/map/map_pane_context_menu.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/qt/settings/ui_settings.hpp>
#include <scwx/qt/ui/about_dialog.hpp>
#include <scwx/qt/ui/alert_dock_widget.hpp>
#include <scwx/qt/ui/animation_dock_widget.hpp>
#include <scwx/qt/ui/collapsible_group.hpp>
#include <scwx/qt/ui/export_settings_dialog.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/ui/gps_info_dialog.hpp>
#include <scwx/qt/ui/imgui_debug_dialog.hpp>
#include <scwx/qt/ui/layer_dialog.hpp>
#include <scwx/qt/ui/map_annotation_dock_widget.hpp>
#include <scwx/qt/ui/level2_products_widget.hpp>
#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/level3_settings_widget.hpp>
#include <scwx/qt/ui/placefile_dialog.hpp>
#include <scwx/qt/ui/marker_dialog.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
#include <scwx/qt/ui/settings_dialog.hpp>
#include <scwx/qt/ui/update_dialog.hpp>
#include <scwx/qt/ui/import/import_settings_wizard.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/products.hpp>
#include <scwx/common/sites.hpp>
#include <scwx/common/vcp.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <set>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QAction>
#include <QActionGroup>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPoint>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

namespace scwx::qt::main
{

static const std::string logPrefix_ = "scwx::qt::main::main_window";
static const auto        logger_    = util::Logger::Create(logPrefix_);

namespace
{
bool AllMapPanesShareSameMapStyle(const std::vector<map::MapWidget*>& maps)
{
   if (maps.size() <= 1u)
   {
      return true;
   }
   const std::string ref = maps.front()->GetMapStyle();
   for (std::size_t i = 1; i < maps.size(); ++i)
   {
      if (maps.at(i)->GetMapStyle() != ref)
      {
         return false;
      }
   }
   return true;
}

bool AllMapPanesReportResolvedMapStyle(const std::vector<map::MapWidget*>& maps)
{
   for (map::MapWidget* m : maps)
   {
      if (m == nullptr)
      {
         continue;
      }
      if (m->GetMapStyle() == "?")
      {
         return false;
      }
   }
   return !maps.empty();
}

bool FindWidgetInGridSplitters(QSplitter*  vs,
                               QWidget*    w,
                               QSplitter** outHs,
                               int*        outIdx)
{
   if (vs == nullptr || w == nullptr || outHs == nullptr || outIdx == nullptr)
   {
      return false;
   }
   for (int r = 0; r < vs->count(); ++r)
   {
      auto* hs = qobject_cast<QSplitter*>(vs->widget(r));
      if (hs == nullptr)
      {
         continue;
      }
      for (int c = 0; c < hs->count(); ++c)
      {
         if (hs->widget(c) == w)
         {
            *outHs  = hs;
            *outIdx = c;
            return true;
         }
      }
   }
   return false;
}

// Row-major, must match |BuildMapLayout| (x inner loop, then next row).
bool MapSlotByMapIndex(QSplitter*   root,
                       std::int64_t gridWidth,
                       std::size_t  mapIndex,
                       QSplitter**  outHs,
                       int*         outIdx)
{
   if (root == nullptr || outHs == nullptr || outIdx == nullptr ||
       gridWidth < 1)
   {
      return false;
   }
   const int row =
      static_cast<int>(mapIndex / static_cast<std::size_t>(gridWidth));
   const int col =
      static_cast<int>(mapIndex % static_cast<std::size_t>(gridWidth));
   if (row < 0 || row >= root->count())
   {
      return false;
   }
   auto* hs = qobject_cast<QSplitter*>(root->widget(row));
   if (hs == nullptr)
   {
      return false;
   }
   if (col < 0 || col >= hs->count())
   {
      return false;
   }
   *outHs  = hs;
   *outIdx = col;
   return true;
}

} // namespace

class MainWindowImpl : public QObject
{
   Q_OBJECT

public:
   explicit MainWindowImpl(MainWindow* mainWindow) :
       mainWindow_ {mainWindow},
       settings_ {},
       activeMap_ {nullptr},
       alertManager_ {manager::AlertManager::Instance()},
       placefileManager_ {manager::PlacefileManager::Instance()},
       markerManager_ {manager::MarkerManager::Instance()},
       positionManager_ {manager::PositionManager::Instance()},
       textEventManager_ {manager::TextEventManager::Instance()},
       timelineManager_ {manager::TimelineManager::Instance()},
       updateManager_ {manager::UpdateManager::Instance()},
       maps_ {}
   {
      mapProvider_ = map::GetMapProvider(
         settings::GeneralSettings::Instance().map_provider().GetValue());
      map::ConfigureMapSettings(mapProvider_, settings_);

      if (settings::GeneralSettings::Instance().track_location().GetValue())
      {
         positionManager_->TrackLocation(true);
      }
   }

   ~MainWindowImpl()
   {
      homeRadarConnection_.disconnect();
      clockFormatConnection_.disconnect();
      defaultTimeZoneConnection_.disconnect();
      for (auto& connection : connections_)
      {
         connection.disconnect();
      }

      clockTimer_.stop();
      threadPool_.join();
   }

   void AddRadarSitePreset(const std::string& id);
   void AsyncSetup();
   void EnsureMapWidgets(int64_t gridWidth, int64_t gridHeight);
   void TeardownMapLayout();
   void BuildMapLayout(bool tryRestorePaneSizesFromSettingsOnSchedule = true);
   void RebuildMapLayoutContainer(
      bool tryRestorePaneSizesFromSettingsOnNextGeometry = true);
   // |tryRestore| false: Reset & Layout — equal panes, ignore saved splitter
   // JSON.
   void RecreateMapLayoutFromUser(
      bool tryRestorePaneSizesFromSettingsOnNextGeometry = true);
   void ConfigureMapLayout();
   void ScheduleMapLayoutSyncIfGridChanged();
   void ApplyRadarProductsFromSettingsToAllMaps();
   void DisconnectMapDataConnections();
   void ConnectMapToTimelineAndRadarSiteSignals();
   void ReconnectMapDataConnections();
   void SetupPanesMenu();
   void UpdatePanesPresetSelection();
   void ApplyMapGridPreset(int64_t gridWidth, int64_t gridHeight);
   void ApplyEqualMapPaneSizes(int layoutRetryDepth = 0);
   void ScheduleMapPaneGeometryApply(
      bool tryRestorePaneSizesFromSettingsFirst = true);
   bool RestoreMapPaneSizesFromSettingsIfMatching();
   void SaveMapPaneSplitterState();
   bool RestoreMapPaneViewLinkFromSettingsIfMatching();
   void SaveMapPaneViewLinkState();
   void SetLinkRowSplitters(bool on);
   void SetLinkColumnHeights(bool on);
   void SnapLinkedColumnWidths();
   void SnapLinkedColumnHeights();
   void ConfigureMapStyles(bool mapStylesIgnoreLiveWidget = false);
   void RestoreAllPanesFromSavedMapSettings();
   void StageMapIndexFromRefWidget(std::size_t           mapIndex,
                                   const map::MapWidget& ref,
                                   bool                  copyMapStyle);
   void ApplyReferencePaneToPendingNewPanes();
   void ConfigureUiSettings();
   void ConnectAnimationSignals();
   void ConnectMapSignals();
   void ConnectOtherSignals();
   void SyncMapPaneViewLinkStateSize();
   void HandleFocusChange(QWidget* focused);
   void InitializeLayerDisplayActions();
   void PopulateCustomMapStyle();
   void PopulateMapStyles();
   void ScreenCapture(types::CaptureType captureType);
   void SelectElevation(map::MapWidget* mapWidget, float elevation);
   void SelectRadarProduct(map::MapWidget*           mapWidget,
                           common::RadarProductGroup group,
                           const std::string&        productName,
                           int16_t                   productCode);
   void ApplyStoredColorTableThreshold(map::MapWidget* mapWidget);
   void HandleMapPaneLinkViewToggled(std::size_t     mapIndex,
                                     map::MapWidget* map,
                                     bool            linked);
   void SetActiveMap(map::MapWidget* mapWidget);
   void UpdateAvailableLevel3Products();
   void UpdateElevationSelection(float elevation);
   void UpdateMapStyle(const std::string& styleName);
   void UpdateRadarProductSelection(common::RadarProductGroup group,
                                    const std::string&        product);
   void UpdateRadarProductSettings();
   void UpdateRadarSite();
   void SelectRadarSiteRespectingViewLink(map::MapWidget*    sourceMap,
                                          const std::string& id,
                                          bool               updateCoordinates);
   void UpdateVcp();
   void UpdateMatchMapStyleFromPanesState(bool allowAutocheck);
   void ApplyMatchMapStyleFromMainToAllPanes();
   void OnPanesMatchMapStyleToggled(bool checked);
   void PopOutMap(std::size_t mapIndex);
   void DockPoppedMap(std::size_t mapIndex);
   void DockAllPoppedBeforeTeardown();
   void SavePoppedMapWindowsToSettings();
   void TryRestorePoppedMapWindows();
   void ConnectMapAnnotationLayerReady(map::MapWidget* mw);
   /// Layer broadcast, floating host resolver, deferred float-from-settings.
   void ConfigureMapAnnotationDock();

   boost::asio::thread_pool threadPool_ {1u};

   MainWindow*         mainWindow_;
   QMapLibre::Settings settings_;
   map::MapProvider    mapProvider_;
   map::MapWidget*     activeMap_;

   std::shared_ptr<gl::GlContext> glContext_ {nullptr};

   ui::CollapsibleGroup*     mapSettingsGroup_ {nullptr};
   ui::CollapsibleGroup*     level2ProductsGroup_ {nullptr};
   ui::CollapsibleGroup*     level2SettingsGroup_ {nullptr};
   ui::CollapsibleGroup*     level3ProductsGroup_ {nullptr};
   ui::CollapsibleGroup*     level3SettingsGroup_ {nullptr};
   ui::CollapsibleGroup*     timelineGroup_ {nullptr};
   ui::Level2ProductsWidget* level2ProductsWidget_ {nullptr};
   ui::Level2SettingsWidget* level2SettingsWidget_ {nullptr};

   ui::Level3ProductsWidget* level3ProductsWidget_ {nullptr};
   ui::Level3SettingsWidget* level3SettingsWidget_ {nullptr};

   QLabel* coordinateLabel_ {nullptr};
   QLabel* timeLabel_ {nullptr};

   ui::AlertDockWidget*                  alertDockWidget_ {};
   QPointer<ui::MapAnnotationDockWidget> mapAnnotationDock_ {};
   ui::AnimationDockWidget*              animationDockWidget_ {};
   ui::AboutDialog*                      aboutDialog_ {};
   ui::ExportSettingsDialog*             exportSettingsDialog_ {};
   ui::GpsInfoDialog*                    gpsInfoDialog_ {};
   ui::ImGuiDebugDialog*                 imGuiDebugDialog_ {};
   ui::import::ImportSettingsWizard*     importSettingsWizard_ {};
   ui::LayerDialog*                      layerDialog_ {};
   ui::PlacefileDialog*                  placefileDialog_ {};
   ui::MarkerDialog*                     markerDialog_ {};
   ui::RadarSiteDialog*                  radarSiteDialog_ {};
   ui::SettingsDialog*                   settingsDialog_ {};
   ui::UpdateDialog*                     updateDialog_ {};

   QTimer clockTimer_ {};

   bool customStyleAvailable_ {false};

   std::vector<boost::signals2::scoped_connection> connections_ {};

#ifdef Q_OS_WIN
   QRect            priorFullScreenGeometry_ {};
   Qt::WindowStates priorFullScreenWindowState_ {};
#endif

   std::shared_ptr<manager::AlertManager>  alertManager_;
   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};
   std::shared_ptr<manager::PlacefileManager> placefileManager_;
   std::shared_ptr<manager::MarkerManager>    markerManager_;
   std::shared_ptr<manager::PositionManager>  positionManager_;
   std::shared_ptr<manager::TextEventManager> textEventManager_;
   std::shared_ptr<manager::TimelineManager>  timelineManager_;
   std::shared_ptr<manager::UpdateManager>    updateManager_;

   std::shared_ptr<model::LayerModel> layerModel_ {
      model::LayerModel::Instance()};
   std::shared_ptr<model::RadarSiteModel> radarSiteModel_ {
      model::RadarSiteModel::Instance()};
   std::map<std::string, std::shared_ptr<QAction>> radarSitePresetsActions_ {};
   QMenu* radarSitePresetsMenu_ {nullptr};

   std::set<std::tuple<types::LayerType, types::LayerDescription, QAction*>>
        layerActions_ {};
   bool layerActionsInitialized_ {false};

   boost::signals2::scoped_connection homeRadarConnection_ {};
   boost::signals2::scoped_connection clockFormatConnection_ {};
   boost::signals2::scoped_connection defaultTimeZoneConnection_ {};

   std::vector<map::MapWidget*> maps_;
   std::vector<std::size_t>     pendingPanesInheritFromRef_;
   // true: map view is synced with other linked panes; false: independent, no
   // broadcast from this pane, no follower updates to this pane.
   std::vector<bool> mapPaneViewLinked_ {};
   std::vector<bool> mapPanePoppedOut_ {};
   // Collapsed slot in the main splitter grid while a pane is in a pop-out
   // window.
   std::vector<QPointer<QWidget>>                    mapPanePlaceholders_ {};
   std::vector<std::unique_ptr<map::MapPopoutFrame>> mapPanePopoutFrames_ {};
   bool popoutPanesRestoredThisSession_ {false};

   QSplitter* mapLayoutRoot_ {nullptr};

   bool linkRowSplitters_ {true};
   bool linkColumnHeights_ {true};

   // Last grid used for the built map splitters; used to detect GeneralSettings
   // changes
   int64_t builtLayoutGridW_ {-1};
   int64_t builtLayoutGridH_ {-1};

   bool mapLayoutGridSyncPending_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

public slots:
   void OnMapPaneContextMenuRequested(const QPoint& globalPos);
   void OnMapAnnotationLayerReady();
   void UpdateMapParameters(double latitude,
                            double longitude,
                            double zoom,
                            double bearing,
                            double pitch);
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    p(std::make_unique<MainWindowImpl>(this)),
    ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   p->InitializeLayerDisplayActions();

   // Assign the bottom left corner to the left dock widget
   setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

   // Configure Radar Site Box
   ui->vcpLabel->setVisible(false);
   ui->vcpValueLabel->setVisible(false);
   ui->vcpDescriptionLabel->setVisible(false);
   ui->saveRadarProductsButton->setVisible(true);

   // QObjects are managed by the parent
   // NOLINTBEGIN(cppcoreguidelines-owning-memory)

   p->radarSitePresetsMenu_ = new QMenu(this);
   ui->radarSitePresetsButton->setMenu(p->radarSitePresetsMenu_);

   auto radarSitePresets = p->radarSiteModel_->presets();
   for (auto& preset : radarSitePresets)
   {
      p->AddRadarSitePreset(preset);
   }

   ui->radarSitePresetsButton->setVisible(!radarSitePresets.empty());

   // Configure Map
   p->ConfigureMapLayout();
   p->SetupPanesMenu();

   const auto defaultTimeZone = p->activeMap_->GetDefaultTimeZone();

   // Configure Alert Dock
   p->alertDockWidget_ = new ui::AlertDockWidget(this);
   addDockWidget(Qt::BottomDockWidgetArea, p->alertDockWidget_);

   p->mapAnnotationDock_ =
      new ui::MapAnnotationDockWidget(p->mainWindow_->ui->centralwidget);
   p->mapAnnotationDock_->AttachToMap(p->activeMap_);
   p->ConfigureMapAnnotationDock();
   for (map::MapWidget* mw : p->maps_)
   {
      if (mw != nullptr)
      {
         p->ConnectMapAnnotationLayerReady(mw);
      }
   }
   if (p->activeMap_ != nullptr)
   {
      p->mapAnnotationDock_->BindToLayer(p->activeMap_->map_annotation_layer(),
                                         false);
   }

   // GPS Info Dialog
   p->gpsInfoDialog_ = new ui::GpsInfoDialog(this);

   // Configure Menu
   ui->menuView->insertAction(ui->actionRadarToolbox,
                              ui->radarToolboxDock->toggleViewAction());
   ui->radarToolboxDock->toggleViewAction()->setText(tr("Radar &Toolbox"));
   ui->actionRadarToolbox->setVisible(false);

   ui->menuView->insertAction(ui->actionAlerts,
                              p->alertDockWidget_->toggleViewAction());
   p->alertDockWidget_->toggleViewAction()->setText(tr("&Alerts"));
   ui->actionAlerts->setVisible(false);

   ui->menuDebug->menuAction()->setVisible(
      settings::GeneralSettings::Instance().debug_enabled().GetValue());

   // Radar Site Dialog
   p->radarSiteDialog_ = new ui::RadarSiteDialog(this);

   // Placefile Manager Dialog
   p->placefileDialog_ = new ui::PlacefileDialog(this);

   // Marker Manager Dialog
   p->markerDialog_ = new ui::MarkerDialog(this);

   // Layer Dialog
   p->layerDialog_ = new ui::LayerDialog(this);

   // Import/Export Dialogs
   p->importSettingsWizard_ = new ui::import::ImportSettingsWizard(this);
   p->exportSettingsDialog_ = new ui::ExportSettingsDialog(this);

   // Settings Dialog
   p->settingsDialog_ = new ui::SettingsDialog(p->settings_, this);

   // Map Settings
   p->mapSettingsGroup_ = new ui::CollapsibleGroup(tr("Map Settings"), this);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(ui->mapStyleLabel);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(ui->mapStyleComboBox);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(
      ui->smoothRadarDataCheckBox);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(
      ui->trackLocationCheckBox);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(
      ui->saveRadarProductsButton);
   ui->radarToolboxScrollAreaContents->layout()->replaceWidget(
      ui->mapSettingsGroupBox, p->mapSettingsGroup_);
   ui->mapSettingsGroupBox->setVisible(false);
   ui->trackLocationCheckBox->setChecked(
      settings::GeneralSettings::Instance().track_location().GetValue());

   // Add Level 2 Products
   p->level2ProductsGroup_ =
      new ui::CollapsibleGroup(tr("Level 2 Products"), this);
   p->level2ProductsWidget_ = new ui::Level2ProductsWidget(this);
   p->level2ProductsGroup_->GetContentsLayout()->addWidget(
      p->level2ProductsWidget_);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(
      p->level2ProductsGroup_);

   // Add Level 3 Products
   p->level3ProductsGroup_ =
      new ui::CollapsibleGroup(tr("Level 3 Products"), this);
   p->level3ProductsWidget_ = new ui::Level3ProductsWidget(this);
   p->level3ProductsGroup_->GetContentsLayout()->addWidget(
      p->level3ProductsWidget_);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(
      p->level3ProductsGroup_);

   // Add Level 2 Settings
   p->level2SettingsGroup_ =
      new ui::CollapsibleGroup(tr("Level 2 Settings"), this);
   p->level2SettingsWidget_ = new ui::Level2SettingsWidget(this);
   p->level2SettingsGroup_->GetContentsLayout()->addWidget(
      p->level2SettingsWidget_);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(
      p->level2SettingsGroup_);
   p->level2SettingsGroup_->setVisible(false);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(
      p->level2SettingsGroup_);

   // Add Level 3 Settings
   p->level3SettingsGroup_ =
      new ui::CollapsibleGroup(tr("Level 3 Settings"), this);
   p->level3SettingsWidget_ = new ui::Level3SettingsWidget(this);
   p->level3SettingsGroup_->GetContentsLayout()->addWidget(
      p->level3SettingsWidget_);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(
      p->level3SettingsGroup_);
   p->level3SettingsGroup_->setVisible(false);

   // Timeline
   p->timelineGroup_       = new ui::CollapsibleGroup(tr("Timeline"), this);
   p->animationDockWidget_ = new ui::AnimationDockWidget(this);
   p->timelineGroup_->GetContentsLayout()->addWidget(p->animationDockWidget_);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(p->timelineGroup_);
   p->animationDockWidget_->UpdateTimeZone(defaultTimeZone);

   // Reset toolbox spacer at the bottom
   ui->radarToolboxScrollAreaContents->layout()->removeItem(
      ui->radarToolboxSpacer);
   ui->radarToolboxScrollAreaContents->layout()->addItem(
      ui->radarToolboxSpacer);

   // Status Bar
   QWidget* statusBarWidget = new QWidget(this);

   p->coordinateLabel_ = new QLabel(this);
   p->coordinateLabel_->setFrameShape(QFrame::Shape::Box);
   p->coordinateLabel_->setFrameShadow(QFrame::Shadow::Sunken);
   p->coordinateLabel_->setVisible(false);

   p->timeLabel_ = new QLabel(this);
   p->timeLabel_->setFrameShape(QFrame::Shape::Box);
   p->timeLabel_->setFrameShadow(QFrame::Shadow::Sunken);
   p->timeLabel_->setVisible(false);

   QGridLayout* statusBarLayout = new QGridLayout(statusBarWidget);
   statusBarLayout->setContentsMargins(0, 0, 0, 0);
   statusBarLayout->addWidget(p->coordinateLabel_, 0, 0);
   statusBarLayout->addWidget(p->timeLabel_, 0, 1);
   ui->statusbar->addPermanentWidget(statusBarWidget);

   // ImGui Debug Dialog
   p->imGuiDebugDialog_ = new ui::ImGuiDebugDialog(this);

   // About Dialog
   p->aboutDialog_ = new ui::AboutDialog(this);

   // Update Dialog
   p->updateDialog_ = new ui::UpdateDialog(this);

   // NOLINTEND(cppcoreguidelines-owning-memory)

   p->ApplyRadarProductsFromSettingsToAllMaps();

   p->PopulateMapStyles();
   p->ConfigureMapStyles();
   if (settings::UiSettings::Instance()
          .panes_match_map_style()
          .GetStagedOrValue())
   {
      p->ApplyMatchMapStyleFromMainToAllPanes();
   }
   p->ConfigureUiSettings();
   p->ConnectMapSignals();
   p->ConnectAnimationSignals();
   p->ConnectOtherSignals();
   p->UpdateMatchMapStyleFromPanesState(false);
   p->HandleFocusChange(p->activeMap_);
   p->AsyncSetup();

   Application::FinishInitialization();
}

MainWindow::~MainWindow()
{
   delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent* ev)
{
   if (p->hotkeyManager_->HandleKeyPress(ev))
   {
      p->activeMap_->update();
      ev->accept();
   }
}

void MainWindow::keyReleaseEvent(QKeyEvent* ev)
{
   if (p->hotkeyManager_->HandleKeyRelease(ev))
   {
      p->activeMap_->update();
      ev->accept();
   }
}

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   static bool firstShowEvent = true;
   bool        restored       = false;

   if (firstShowEvent)
   {
      auto& uiSettings = settings::UiSettings::Instance();

      // restore the geometry state
      const std::string uiGeometry = uiSettings.main_ui_geometry().GetValue();
      restoreGeometry(
         QByteArray::fromBase64(QByteArray::fromStdString(uiGeometry)));

      // restore the UI state
      const std::string uiState = uiSettings.main_ui_state().GetValue();

      restored = restoreState(
         QByteArray::fromBase64(QByteArray::fromStdString(uiState)));

      firstShowEvent = false;
   }

   if (!restored)
   {
      resizeDocks({ui->radarToolboxDock}, {194}, Qt::Horizontal);
   }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
   auto& uiSettings = settings::UiSettings::Instance();

   // save the UI geometry
   QByteArray uiGeometry = saveGeometry().toBase64();
   uiSettings.main_ui_geometry().StageValue(uiGeometry.data());

   // save the UI state
   QByteArray uiState = saveState().toBase64();
   uiSettings.main_ui_state().StageValue(uiState.data());

   p->SavePoppedMapWindowsToSettings();
   p->DockAllPoppedBeforeTeardown();
   p->SaveMapPaneSplitterState();
   p->SaveMapPaneViewLinkState();

   QMainWindow::closeEvent(event);
}

void MainWindow::on_actionOpenNexrad_triggered()
{
   static const std::string nexradFilter = "NEXRAD Products (*)";

   QFileDialog* dialog = new QFileDialog(this);

   dialog->setFileMode(QFileDialog::ExistingFile);
   dialog->setNameFilter(tr(nexradFilter.c_str()));
   dialog->setAttribute(Qt::WA_DeleteOnClose);

   map::MapWidget* currentMap = p->activeMap_;

   // Make sure the parent window properly repaints on close
   connect(dialog,
           &QFileDialog::finished,
           this,
           static_cast<void (MainWindow::*)()>(&MainWindow::update),
           Qt::QueuedConnection);

   connect(
      dialog,
      &QFileDialog::fileSelected,
      this,
      [=, this](const QString& file)
      {
         logger_->info("Selected: {}", file.toStdString());

         auto        radarSite = p->activeMap_->GetRadarSite();
         std::string currentRadarSite =
            (radarSite != nullptr) ? radarSite->id() : std::string {};

         std::shared_ptr<request::NexradFileRequest> request =
            std::make_shared<request::NexradFileRequest>(currentRadarSite);

         connect( //
            request.get(),
            &request::NexradFileRequest::RequestComplete,
            this,
            [=, this](std::shared_ptr<request::NexradFileRequest> request)
            {
               std::shared_ptr<types::RadarProductRecord> record =
                  request->radar_product_record();

               if (record != nullptr)
               {
                  currentMap->SetAutoRefresh(false);
                  currentMap->SelectRadarProduct(record);
               }
               else
               {
                  QMessageBox* messageBox = new QMessageBox(this);
                  messageBox->setIcon(QMessageBox::Warning);
                  messageBox->setText(
                     QString("%1\n%2").arg(tr("Unrecognized NEXRAD Product:"),
                                           QDir::toNativeSeparators(file)));
                  messageBox->setAttribute(Qt::WA_DeleteOnClose);
                  messageBox->open();
               }
            });

         manager::RadarProductManager::LoadFile(file.toStdString(), request);
      });

   dialog->open();
}

void MainWindow::on_actionOpenTextEvent_triggered()
{
   static const std::string textFilter = "Text Event Products (*.txt)";
   static const std::string allFilter  = "All Files (*)";

   QFileDialog* dialog = new QFileDialog(this);

   dialog->setFileMode(QFileDialog::ExistingFile);
   dialog->setNameFilters({tr(textFilter.c_str()), tr(allFilter.c_str())});
   dialog->setAttribute(Qt::WA_DeleteOnClose);

   // Make sure the parent window properly repaints on close
   connect(dialog,
           &QFileDialog::finished,
           this,
           static_cast<void (MainWindow::*)()>(&MainWindow::update),
           Qt::QueuedConnection);

   connect(dialog,
           &QFileDialog::fileSelected,
           this,
           [this](const QString& file)
           {
              logger_->info("Selected: {}", file.toStdString());
              p->textEventManager_->LoadFile(file.toStdString());
           });

   dialog->open();
}

void MainWindow::on_actionSaveNexrad_triggered()
{
   map::MapWidget* currentMap = p->activeMap_;
   if (currentMap == nullptr)
   {
      return;
   }

   auto radarSite = currentMap->GetRadarSite();
   if (radarSite == nullptr)
   {
      QMessageBox::warning(this,
                           tr("Save NEXRAD Product"),
                           tr("No radar site is selected."));
      return;
   }

   auto manager = manager::RadarProductManager::Instance(radarSite->id());
   auto record  = manager->GetRadarProductRecord(
      currentMap->GetRadarProductGroup(),
      currentMap->GetRadarProductName(),
      currentMap->GetSelectedTime());

   auto nexradFile = (record != nullptr) ? record->nexrad_file() : nullptr;
   if (nexradFile == nullptr || !nexradFile->has_file_data())
   {
      QMessageBox::warning(
         this,
         tr("Save NEXRAD Product"),
         tr("The currently displayed radar product is not available as a "
            "complete NEXRAD file. This can happen while a live Level 2 volume "
            "is still being received. Select a completed historical volume, "
            "then try again."));
      return;
   }

   static const std::string nexradFilter = "NEXRAD Products (*)";

   QFileDialog* dialog = new QFileDialog(this);

   dialog->setAcceptMode(QFileDialog::AcceptSave);
   dialog->setFileMode(QFileDialog::AnyFile);
   dialog->setNameFilter(tr(nexradFilter.c_str()));
   dialog->setAttribute(Qt::WA_DeleteOnClose);
   dialog->selectFile(QString::fromStdString(record->suggested_filename()));

   // Make sure the parent window properly repaints on close
   connect(dialog,
           &QFileDialog::finished,
           this,
           static_cast<void (MainWindow::*)()>(&MainWindow::update),
           Qt::QueuedConnection);

   connect(
      dialog,
      &QFileDialog::fileSelected,
      this,
      [this, nexradFile](const QString& file)
      {
         const std::string filename =
            QDir::toNativeSeparators(file).toStdString();
         logger_->info("Saving NEXRAD product: {}", filename);

         if (!nexradFile->SaveFile(filename))
         {
            QMessageBox::critical(
               this,
               tr("Save NEXRAD Product"),
               tr("Unable to save NEXRAD product to %1.")
                  .arg(QDir::toNativeSeparators(file)));
         }
      });

   dialog->open();
}

void MainWindow::on_actionScreenCaptureCopy_triggered()
{
   p->ScreenCapture(types::CaptureType::Copy);
}

void MainWindow::on_actionScreenCaptureSaveImage_triggered()
{
   p->ScreenCapture(types::CaptureType::SaveImage);
}

void MainWindow::on_actionImport_triggered()
{
   p->importSettingsWizard_->show();
}

void MainWindow::on_actionExport_triggered()
{
   p->exportSettingsDialog_->show();
}

void MainWindow::on_actionSettings_triggered()
{
   p->settingsDialog_->show();
}

void MainWindow::on_actionExit_triggered()
{
   close();
}

void MainWindow::on_actionGpsInfo_triggered()
{
   p->gpsInfoDialog_->show();
}

void MainWindow::on_actionRecreateMapLayout_triggered()
{
   p->RecreateMapLayoutFromUser(false);
}

void MainWindow::on_actionPanesLinkColumnWidth_toggled(bool checked)
{
   p->SetLinkRowSplitters(checked);
}

void MainWindow::on_actionPanesLinkColumnHeight_toggled(bool checked)
{
   p->SetLinkColumnHeights(checked);
}

void MainWindow::on_actionPanesMatchMapStyle_toggled(bool checked)
{
   p->OnPanesMatchMapStyleToggled(checked);
}

void MainWindow::on_actionPanes1x1_triggered()
{
   p->ApplyMapGridPreset(1, 1);
}

void MainWindow::on_actionPanes1x2_triggered()
{
   p->ApplyMapGridPreset(1, 2);
}

void MainWindow::on_actionPanes2x1_triggered()
{
   p->ApplyMapGridPreset(2, 1);
}

void MainWindow::on_actionPanes2x2_triggered()
{
   p->ApplyMapGridPreset(2, 2);
}

void MainWindow::on_actionPanes3x3_triggered()
{
   p->ApplyMapGridPreset(3, 3);
}

void MainWindow::on_actionPanesCustom_triggered()
{
   p->settingsDialog_->show();
}

void MainWindow::on_actionColorTable_triggered(bool checked)
{
   p->layerModel_->SetLayerDisplayed(types::LayerType::Information,
                                     types::InformationLayer::ColorTable,
                                     checked);
}

void MainWindow::on_actionRadarRange_triggered(bool checked)
{
   p->layerModel_->SetLayerDisplayed(
      types::LayerType::Data, types::DataLayer::RadarRange, checked);
}

void MainWindow::on_actionRadarSites_triggered(bool checked)
{
   p->layerModel_->SetLayerDisplayed(types::LayerType::Information,
                                     types::InformationLayer::RadarSite,
                                     checked);
}

void MainWindow::on_actionPlacefileManager_triggered()
{
   p->placefileDialog_->show();
}

void MainWindow::on_actionMarkerManager_triggered()
{
   p->markerDialog_->show();
}

void MainWindow::on_actionLayerManager_triggered()
{
   p->layerDialog_->show();
}

void MainWindow::on_actionImGuiDebug_triggered()
{
   p->imGuiDebugDialog_->show();
}

void MainWindow::on_actionDumpLayerList_triggered()
{
   p->activeMap_->DumpLayerList();
}

void MainWindow::on_actionDumpRadarProductRecords_triggered()
{
   manager::RadarProductManager::DumpRecords();
}

void MainWindow::on_actionFullScreen_triggered(bool checked)
{
   if (checked)
   {
#ifdef Q_OS_WIN
      // On Windows, showFullScreen() with QOpenGLWidgets breaks dropdown menus.
      // Use a frameless window covering the screen geometry as a workaround.
      p->priorFullScreenWindowState_ = windowState();
      p->priorFullScreenGeometry_    = geometry();
      setWindowFlag(Qt::FramelessWindowHint, true);
      QScreen* screen = windowHandle() ? windowHandle()->screen() : nullptr;
      if (screen == nullptr)
      {
         screen = QGuiApplication::primaryScreen();
      }
      if (screen != nullptr)
      {
         setGeometry(screen->geometry());
      }
      show();
#else
      showFullScreen();
#endif
   }
   else
   {
#ifdef Q_OS_WIN
      setWindowFlag(Qt::FramelessWindowHint, false);
      if (p->priorFullScreenWindowState_ & Qt::WindowMaximized)
      {
         showMaximized();
      }
      else
      {
         showNormal();
         setGeometry(p->priorFullScreenGeometry_);
      }
#else
      setWindowState(windowState() & ~Qt::WindowFullScreen);
#endif
   }
}

void MainWindow::on_actionRadarWireframe_triggered(bool checked)
{
   p->activeMap_->SetRadarWireframeEnabled(checked);
}

void MainWindow::on_actionUserManual_triggered()
{
   QDesktopServices::openUrl(QUrl {"https://supercell-wx.readthedocs.io/"});
}

void MainWindow::on_actionDiscord_triggered()
{
   QDesktopServices::openUrl(QUrl {"https://discord.gg/vFMV76brwU"});
}

void MainWindow::on_actionGitHubRepository_triggered()
{
   QDesktopServices::openUrl(QUrl {"https://github.com/dpaulat/supercell-wx"});
}

void MainWindow::on_actionCheckForUpdates_triggered()
{
   boost::asio::post(
      p->threadPool_,
      [this]()
      {
         try
         {
            if (!p->updateManager_->CheckForUpdates(main::kVersionString_))
            {
               QMetaObject::invokeMethod(
                  this,
                  [this]()
                  {
                     QMessageBox* messageBox = new QMessageBox(this);
                     messageBox->setIcon(QMessageBox::Icon::Information);
                     messageBox->setWindowTitle(tr("Check for Updates"));
                     messageBox->setText(tr("Supercell Wx is up to date."));
                     messageBox->setStandardButtons(
                        QMessageBox::StandardButton::Ok);
                     messageBox->show();
                  });
            }
         }
         catch (const std::exception& ex)
         {
            logger_->error(ex.what());
         }
      });
}

void MainWindow::on_actionAboutSupercellWx_triggered()
{
   p->aboutDialog_->show();
}

void MainWindow::on_radarSiteHomeButton_clicked()
{
   std::string homeRadarSite =
      settings::GeneralSettings::Instance().default_radar_site().GetValue();

   for (map::MapWidget* map : p->maps_)
   {
      map->SelectRadarSite(homeRadarSite);
   }

   p->UpdateRadarSite();
   p->UpdateAvailableLevel3Products();
   p->UpdateRadarProductSettings();
}

void MainWindow::on_radarSiteSelectButton_clicked()
{
   p->radarSiteDialog_->show();
}

void MainWindowImpl::AsyncSetup()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   // Check for updates
   if (generalSettings.update_notifications_enabled().GetValue())
   {
      boost::asio::post(threadPool_,
                        [this]()
                        {
                           try
                           {
                              manager::UpdateManager::RemoveTemporaryReleases();
                              updateManager_->CheckForUpdates(
                                 main::kVersionString_);
                           }
                           catch (const std::exception& ex)
                           {
                              logger_->error(ex.what());
                           }
                        });
   }
}

void MainWindowImpl::EnsureMapWidgets(int64_t gridWidth, int64_t gridHeight)
{
   const int64_t mapCount      = gridWidth * gridHeight;
   const bool hadReferencePane = (!maps_.empty() && maps_.front() != nullptr);

   pendingPanesInheritFromRef_.clear();

   if (static_cast<int64_t>(maps_.size()) > mapCount)
   {
      SyncMapPaneViewLinkStateSize();
      while (static_cast<int64_t>(maps_.size()) > mapCount)
      {
         const std::size_t last = maps_.size() - 1u;
         if (last < mapPanePoppedOut_.size() && mapPanePoppedOut_.at(last))
         {
            DockPoppedMap(last);
         }
         map::MapWidget* const lastMap = maps_.back();
         if (lastMap == activeMap_)
         {
            activeMap_ = nullptr;
         }
         if (!mapAnnotationDock_.isNull())
         {
            mapAnnotationDock_->DetachIfHostedBy(lastMap);
         }
         // MapWidget not managed by smart ptr; Qt widget lifetime via parent
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         delete lastMap;
         maps_.pop_back();
      }
   }
   else
   {
      maps_.resize(mapCount, nullptr);
   }

   if (!glContext_)
   {
      glContext_ = std::make_shared<gl::GlContext>();
   }

   for (int64_t i = 0; i < mapCount; i++)
   {
      if (maps_.at(i) == nullptr)
      {
         if (i > 0 && hadReferencePane && maps_.at(0) != nullptr)
         {
            pendingPanesInheritFromRef_.push_back(static_cast<std::size_t>(i));
         }
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Owned by parent
         maps_.at(i) = new map::MapWidget(
            static_cast<std::size_t>(i), settings_, glContext_);
         ConnectMapAnnotationLayerReady(maps_.at(i));
      }
   }

   SyncMapPaneViewLinkStateSize();
}

void MainWindowImpl::ConnectMapAnnotationLayerReady(map::MapWidget* mw)
{
   if (mw == nullptr)
   {
      return;
   }
   static_cast<void>(
      QObject::connect(mw,
                       &map::MapWidget::MapAnnotationLayerReady,
                       this,
                       &MainWindowImpl::OnMapAnnotationLayerReady,
                       Qt::UniqueConnection));
}

void MainWindowImpl::OnMapAnnotationLayerReady()
{
   auto* const mw = qobject_cast<map::MapWidget*>(sender());
   if (mw == nullptr || mapAnnotationDock_.isNull())
   {
      return;
   }
   if (mw == activeMap_)
   {
      mapAnnotationDock_->BindToLayer(activeMap_->map_annotation_layer(),
                                      false);
   }
   else
   {
      mapAnnotationDock_->ReapplyToolAndStyleFromUi();
   }
}

void MainWindowImpl::ConfigureMapAnnotationDock()
{
   if (mapAnnotationDock_.isNull())
   {
      return;
   }
   mapAnnotationDock_->SetBroadcastTargets(
      [this]()
      {
         std::vector<std::shared_ptr<map::MapAnnotationLayer>> layers;
         for (map::MapWidget* mw : maps_)
         {
            if (mw == nullptr)
            {
               continue;
            }
            if (auto layer = mw->map_annotation_layer())
            {
               layers.push_back(std::move(layer));
            }
         }
         return layers;
      });
   mapAnnotationDock_->SetFloatingDockHostResolver(
      [this]() -> QWidget*
      {
         if (activeMap_ != nullptr)
         {
            return activeMap_;
         }
         for (map::MapWidget* mw : maps_)
         {
            if (mw != nullptr)
            {
               return mw;
            }
         }
         return nullptr;
      });
   mapAnnotationDock_->ApplyDeferredFloatingState();
}

void MainWindowImpl::SyncMapPaneViewLinkStateSize()
{
   mapPaneViewLinked_.resize(maps_.size(), true);
   mapPanePoppedOut_.resize(maps_.size(), false);
   mapPanePlaceholders_.resize(maps_.size(), nullptr);
   mapPanePopoutFrames_.resize(maps_.size());
}

void MainWindowImpl::SelectRadarSiteRespectingViewLink(
   map::MapWidget* sourceMap, const std::string& id, bool updateCoordinates)
{
   // With "Link view" on, a radar pick should match other linked panes; with it
   // off, only that pane changes site. nullptr = broadcast (dialog, toolbar
   // preset): every pane, same as historical behavior.
   if (sourceMap == nullptr)
   {
      for (map::MapWidget* map : maps_)
      {
         if (map != nullptr)
         {
            map->SelectRadarSite(id, updateCoordinates);
         }
      }
      return;
   }

   SyncMapPaneViewLinkStateSize();

   const auto sit = std::ranges::find(maps_, sourceMap);
   if (sit == maps_.end())
   {
      sourceMap->SelectRadarSite(id, updateCoordinates);
      return;
   }

   const auto sourceIndex =
      static_cast<std::size_t>(std::distance(maps_.begin(), sit));
   if (sourceIndex < mapPaneViewLinked_.size() &&
       !mapPaneViewLinked_.at(sourceIndex))
   {
      sourceMap->SelectRadarSite(id, updateCoordinates);
      return;
   }

   for (std::size_t i = 0; i < maps_.size(); ++i)
   {
      if (i == sourceIndex ||
          (i < mapPaneViewLinked_.size() && mapPaneViewLinked_.at(i)))
      {
         maps_.at(i)->SelectRadarSite(id, updateCoordinates);
      }
   }
}

void MainWindowImpl::TeardownMapLayout()
{
   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }

   DockAllPoppedBeforeTeardown();

   for (map::MapWidget* map : maps_)
   {
      if (map != nullptr)
      {
         map->setParent(nullptr);
      }
   }

   QLayout* const centralLayout = mainWindow_->ui->centralwidget->layout();
   if (centralLayout != nullptr)
   {
      centralLayout->removeWidget(mapLayoutRoot_);
   }

   // Nested QSplitters (not MapWidgets) are children of |mapLayoutRoot_|
   delete mapLayoutRoot_;
   mapLayoutRoot_ = nullptr;
}

void MainWindowImpl::BuildMapLayout(
   bool tryRestorePaneSizesFromSettingsOnSchedule)
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   const int64_t gridWidth  = generalSettings.grid_width().GetValue();
   const int64_t gridHeight = generalSettings.grid_height().GetValue();
   const int64_t mapCount   = gridWidth * gridHeight;

   EnsureMapWidgets(gridWidth, gridHeight);
   timelineManager_->SetMapCount(mapCount);

   map::MapWidget* const priorActive = activeMap_;

   size_t mapIndex = 0;

   // Parent splitter so the widget tree is well-formed before addWidget
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Central layout owns
   auto* vs = new QSplitter(Qt::Vertical, mainWindow_->ui->centralwidget);
   vs->setHandleWidth(1);
   mapLayoutRoot_ = vs;

   auto MoveSplitter = [this, vs](int /*pos*/, int /*index*/)
   {
      if (!linkRowSplitters_)
      {
         return;
      }
      if (std::any_of(mapPanePoppedOut_.cbegin(),
                      mapPanePoppedOut_.cend(),
                      [](bool p) { return p; }))
      {
         return;
      }
      QSplitter* s = static_cast<QSplitter*>(sender());
      if (vs->indexOf(s) < 0)
      {
         return;
      }
      const auto sizes = s->sizes();
      int        sum   = 0;
      for (const int x : sizes)
      {
         sum += x;
      }
      if (sum < 1)
      {
         return;
      }
      for (int r = 0; r < vs->count(); ++r)
      {
         if (auto* rowHs = qobject_cast<QSplitter*>(vs->widget(r)))
         {
            if (rowHs->count() == sizes.size())
            {
               rowHs->setSizes(sizes);
            }
         }
      }
   };
   auto MoveVerticalSplitter = [this](int /*pos*/, int /*index*/)
   {
      if (linkColumnHeights_)
      {
         SnapLinkedColumnHeights();
      }
   };
   connect(vs, &QSplitter::splitterMoved, this, MoveVerticalSplitter);

   for (int64_t y = 0; y < gridHeight; y++)
   {
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): mapLayoutRoot_ owns
      QSplitter* hs = new QSplitter(vs);
      hs->setHandleWidth(1);

      for (int64_t x = 0; x < gridWidth; x++, mapIndex++)
      {
         hs->addWidget(maps_.at(mapIndex));
      }

      connect(hs, &QSplitter::splitterMoved, this, MoveSplitter);
   }

   mainWindow_->ui->centralwidget->layout()->addWidget(vs);

   if (mapCount > 0)
   {
      if (priorActive != nullptr &&
          std::find(maps_.cbegin(), maps_.cend(), priorActive) != maps_.cend())
      {
         SetActiveMap(priorActive);
      }
      else
      {
         SetActiveMap(maps_.at(0));
      }
   }
   else
   {
      SetActiveMap(nullptr);
   }

   builtLayoutGridW_ = gridWidth;
   builtLayoutGridH_ = gridHeight;

   RestoreMapPaneViewLinkFromSettingsIfMatching();
   ScheduleMapPaneGeometryApply(tryRestorePaneSizesFromSettingsOnSchedule);
}

void MainWindowImpl::RebuildMapLayoutContainer(
   bool tryRestorePaneSizesFromSettingsOnNextGeometry)
{
   TeardownMapLayout();
   BuildMapLayout(tryRestorePaneSizesFromSettingsOnNextGeometry);
}

void MainWindowImpl::RecreateMapLayoutFromUser(
   bool tryRestorePaneSizesFromSettingsOnNextGeometry)
{
   RebuildMapLayoutContainer(tryRestorePaneSizesFromSettingsOnNextGeometry);
   ReconnectMapDataConnections();
   ApplyRadarProductsFromSettingsToAllMaps();
   ConfigureMapStyles();
   ApplyReferencePaneToPendingNewPanes();
   if (settings::UiSettings::Instance()
          .panes_match_map_style()
          .GetStagedOrValue())
   {
      ApplyMatchMapStyleFromMainToAllPanes();
   }
   UpdateMatchMapStyleFromPanesState(false);
   layerDialog_->RefreshMapDisplayColumns();
   UpdateAvailableLevel3Products();
   UpdateRadarProductSettings();
   UpdateRadarSite();
   UpdateVcp();
   if (activeMap_ != nullptr)
   {
      HandleFocusChange(activeMap_);
   }
   UpdatePanesPresetSelection();
}

void MainWindowImpl::ConfigureMapLayout()
{
   RebuildMapLayoutContainer();
}

void MainWindowImpl::ScheduleMapLayoutSyncIfGridChanged()
{
   if (mapLayoutGridSyncPending_)
   {
      return;
   }
   mapLayoutGridSyncPending_ = true;
   QTimer::singleShot(0,
                      this,
                      [this]()
                      {
                         mapLayoutGridSyncPending_ = false;
                         const auto& g = settings::GeneralSettings::Instance();
                         const int64_t gw = g.grid_width().GetValue();
                         const int64_t gh = g.grid_height().GetValue();
                         if (gw != builtLayoutGridW_ || gh != builtLayoutGridH_)
                         {
                            RecreateMapLayoutFromUser();
                         }
                      });
}

void MainWindowImpl::ApplyRadarProductsFromSettingsToAllMaps()
{
   auto& mapSettings = settings::MapSettings::Instance();
   for (std::size_t i = 0; i < maps_.size(); i++)
   {
      if (maps_.at(i) == nullptr)
      {
         continue;
      }
      SelectRadarProduct(
         maps_.at(i),
         common::GetRadarProductGroup(
            mapSettings.radar_product_group(i).GetStagedOrValue()),
         mapSettings.radar_product(i).GetStagedOrValue(),
         0);
   }
}

void MainWindowImpl::ScheduleMapPaneGeometryApply(
   bool tryRestorePaneSizesFromSettingsFirst)
{
   const QPointer<QSplitter> vsGuard {mapLayoutRoot_};
   QTimer::singleShot(
      0,
      this,
      [this, vsGuard, tryRestorePaneSizesFromSettingsFirst]()
      {
         if (vsGuard == nullptr)
         {
            return;
         }
         if (mapLayoutRoot_ == nullptr || mapLayoutRoot_ != vsGuard)
         {
            return;
         }
         const bool hasPoppedPane =
            std::any_of(mapPanePoppedOut_.cbegin(),
                        mapPanePoppedOut_.cend(),
                        [](bool popped) { return popped; });
         if (!hasPoppedPane && tryRestorePaneSizesFromSettingsFirst &&
             RestoreMapPaneSizesFromSettingsIfMatching())
         {
            TryRestorePoppedMapWindows();
            return;
         }
         ApplyEqualMapPaneSizes();
         TryRestorePoppedMapWindows();
      });
}

void MainWindowImpl::ApplyEqualMapPaneSizes(int layoutRetryDepth)
{
   static constexpr int kMaxEqualizeLayoutRetries = 8;

   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }

   QSplitter* const vs    = mapLayoutRoot_;
   const int        nRows = vs->count();
   if (nRows <= 0)
   {
      return;
   }
   if (vs->height() < 2 || vs->width() < 2)
   {
      if (layoutRetryDepth < kMaxEqualizeLayoutRetries)
      {
         const QPointer<QObject> self {this};
         QTimer::singleShot(0,
                            this,
                            [this, self, layoutRetryDepth]()
                            {
                               if (self)
                               {
                                  ApplyEqualMapPaneSizes(layoutRetryDepth + 1);
                               }
                            });
      }
      return;
   }
   const bool hasPoppedPane = std::any_of(mapPanePoppedOut_.cbegin(),
                                          mapPanePoppedOut_.cend(),
                                          [](bool popped) { return popped; });
   if (!hasPoppedPane)
   {
      for (int r = 0; r < nRows; ++r)
      {
         auto* const rowHs = qobject_cast<QSplitter*>(vs->widget(r));
         if (rowHs != nullptr && rowHs->width() < 2)
         {
            if (layoutRetryDepth < kMaxEqualizeLayoutRetries)
            {
               const QPointer<QObject> self {this};
               QTimer::singleShot(0,
                                  this,
                                  [this, self, layoutRetryDepth]()
                                  {
                                     if (self)
                                     {
                                        ApplyEqualMapPaneSizes(
                                           layoutRetryDepth + 1);
                                     }
                                  });
            }
            return;
         }
      }
   }

   const int totalH = std::max(1, vs->height());
   if (hasPoppedPane && builtLayoutGridW_ > 0)
   {
      QList<int>        vert;
      int               visibleRowCount = 0;
      std::vector<bool> rowVisible(static_cast<std::size_t>(nRows), false);
      for (int r = 0; r < nRows; ++r)
      {
         bool rowHasVisiblePane = false;
         for (int c = 0; c < builtLayoutGridW_; ++c)
         {
            const auto mapIndex =
               static_cast<std::size_t>(r * builtLayoutGridW_ + c);
            if (mapIndex < maps_.size() &&
                (mapIndex >= mapPanePoppedOut_.size() ||
                 !mapPanePoppedOut_.at(mapIndex)))
            {
               rowHasVisiblePane = true;
               break;
            }
         }
         if (rowHasVisiblePane)
         {
            ++visibleRowCount;
            rowVisible.at(static_cast<std::size_t>(r)) = true;
         }
      }
      if (visibleRowCount == 0)
      {
         QList<int> allPoppedRowSizes;
         for (int r = 0; r < nRows; ++r)
         {
            if (QWidget* rowWidget = vs->widget(r); rowWidget != nullptr)
            {
               rowWidget->setVisible(true);
            }
            vs->setCollapsible(r, false);
            vs->setStretchFactor(r, 1);
            allPoppedRowSizes.append(1);

            auto* const hs = qobject_cast<QSplitter*>(vs->widget(r));
            if (hs == nullptr)
            {
               continue;
            }
            for (int c = 0; c < hs->count(); ++c)
            {
               if (QWidget* colWidget = hs->widget(c); colWidget != nullptr)
               {
                  colWidget->setVisible(true);
               }
               hs->setCollapsible(c, false);
               hs->setStretchFactor(c, 1);
            }
            QList<int> col;
            for (int c = 0; c < hs->count(); ++c)
            {
               col.append(1);
            }
            hs->setSizes(col);
         }
         vs->setSizes(allPoppedRowSizes);
         return;
      }

      int remainingH         = totalH;
      int emittedVisibleRows = 0;
      for (int r = 0; r < nRows; ++r)
      {
         const bool rowHasVisiblePane =
            rowVisible.at(static_cast<std::size_t>(r));
         if (QWidget* rowWidget = vs->widget(r); rowWidget != nullptr)
         {
            // Keep splitter children visible. Hiding rows/cells survives app
            // restore poorly and can keep newly docked maps invisible.
            rowWidget->setVisible(true);
         }
         vs->setCollapsible(r, !rowHasVisiblePane);
         vs->setStretchFactor(r, rowHasVisiblePane ? 1 : 0);
         if (!rowHasVisiblePane || visibleRowCount == 0)
         {
            vert.append(1);
            continue;
         }
         ++emittedVisibleRows;
         const int part = (emittedVisibleRows < visibleRowCount) ?
                             (totalH / visibleRowCount) :
                             remainingH;
         vert.append(part);
         remainingH -= part;
      }
      vs->setSizes(vert);

      for (int r = 0; r < nRows; ++r)
      {
         auto* const hs = qobject_cast<QSplitter*>(vs->widget(r));
         if (hs == nullptr)
         {
            continue;
         }
         const int nCol = hs->count();
         if (nCol <= 0)
         {
            continue;
         }
         int               visibleColCount = 0;
         std::vector<bool> colVisible(static_cast<std::size_t>(nCol), false);
         for (int c = 0; c < nCol; ++c)
         {
            const auto mapIndex =
               static_cast<std::size_t>(r * builtLayoutGridW_ + c);
            if (mapIndex < maps_.size() &&
                (mapIndex >= mapPanePoppedOut_.size() ||
                 !mapPanePoppedOut_.at(mapIndex)))
            {
               ++visibleColCount;
               colVisible.at(static_cast<std::size_t>(c)) = true;
            }
         }

         const int  totalW             = std::max(1, hs->width());
         int        remainingW         = totalW;
         int        emittedVisibleCols = 0;
         QList<int> col;
         for (int c = 0; c < nCol; ++c)
         {
            const bool visible = colVisible.at(static_cast<std::size_t>(c));
            if (QWidget* colWidget = hs->widget(c); colWidget != nullptr)
            {
               colWidget->setVisible(true);
            }
            hs->setCollapsible(c, !visible);
            hs->setStretchFactor(c, visible ? 1 : 0);
            if (!visible || visibleColCount == 0)
            {
               col.append(1);
               continue;
            }
            ++emittedVisibleCols;
            const int part = (emittedVisibleCols < visibleColCount) ?
                                (totalW / visibleColCount) :
                                remainingW;
            col.append(part);
            remainingW -= part;
         }
         hs->setSizes(col);
      }
      if (layoutRetryDepth < kMaxEqualizeLayoutRetries)
      {
         const QPointer<QObject> self {this};
         QTimer::singleShot(0,
                            this,
                            [this, self, layoutRetryDepth]()
                            {
                               if (self)
                               {
                                  ApplyEqualMapPaneSizes(layoutRetryDepth + 1);
                               }
                            });
      }
      return;
   }

   {
      const int  base = totalH / nRows;
      int        acc  = 0;
      QList<int> vert;
      for (int r = 0; r < nRows; ++r)
      {
         if (QWidget* rowWidget = vs->widget(r); rowWidget != nullptr)
         {
            rowWidget->setVisible(true);
            rowWidget->setMinimumHeight(0);
            rowWidget->setMaximumHeight(QWIDGETSIZE_MAX);
            rowWidget->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Expanding);
         }
         vs->setCollapsible(r, false);
         vs->setStretchFactor(r, 1);
         const int part = (r < nRows - 1) ? base : (totalH - acc);
         vert.append(part);
         acc += part;
      }
      vs->setSizes(vert);
   }

   for (int r = 0; r < nRows; ++r)
   {
      QWidget* w = vs->widget(r);
      if (w == nullptr)
      {
         continue;
      }
      auto* hs = qobject_cast<QSplitter*>(w);
      if (hs == nullptr)
      {
         continue;
      }
      const int nCol = hs->count();
      if (nCol <= 0)
      {
         continue;
      }
      for (int c = 0; c < nCol; ++c)
      {
         if (QWidget* colWidget = hs->widget(c); colWidget != nullptr)
         {
            colWidget->setVisible(true);
            colWidget->setMinimumWidth(0);
            colWidget->setMaximumWidth(QWIDGETSIZE_MAX);
            colWidget->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Expanding);
         }
         hs->setCollapsible(c, false);
         hs->setStretchFactor(c, 1);
      }
      const int  totalW = std::max(1, hs->width());
      const int  baseW  = totalW / nCol;
      int        accW   = 0;
      QList<int> col;
      for (int c = 0; c < nCol; ++c)
      {
         const int partW = (c < nCol - 1) ? baseW : (totalW - accW);
         col.append(partW);
         accW += partW;
      }
      hs->setSizes(col);
   }
}

bool MainWindowImpl::RestoreMapPaneSizesFromSettingsIfMatching()
{
   if (mapLayoutRoot_ == nullptr)
   {
      return false;
   }
   if (mapLayoutRoot_->width() < 2 || mapLayoutRoot_->height() < 2)
   {
      // |map_pane_splitter_state| is absolute pixel sizes. Applying it before
      // the central |QSplitter| has a real geometry (e.g. right after a
      // pop-out) can collapse or starve rows; fall back to equal sizing once
      // layout is valid.
      return false;
   }

   const std::string json =
      settings::UiSettings::Instance().map_pane_splitter_state().GetValue();
   if (json.empty())
   {
      return false;
   }

   QJsonParseError     err {};
   const QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(json), &err);
   if (err.error != QJsonParseError::NoError || !doc.isObject())
   {
      return false;
   }

   const QJsonObject o = doc.object();
   if (!o.contains("gw") || !o.contains("gh"))
   {
      return false;
   }

   const int64_t jgw = static_cast<int64_t>(o.value("gw").toDouble(0.0));
   const int64_t jgh = static_cast<int64_t>(o.value("gh").toDouble(0.0));
   const int64_t sgw = builtLayoutGridW_;
   const int64_t sgh = builtLayoutGridH_;
   if (jgw != sgw || jgh != sgh || sgw < 1 || sgh < 1)
   {
      return false;
   }

   const QJsonValue vval = o.value("v");
   if (!vval.isArray())
   {
      return false;
   }
   const QJsonArray varr  = vval.toArray();
   const QJsonValue rrows = o.value("rows");
   if (!rrows.isArray())
   {
      return false;
   }
   const QJsonArray rowArr = rrows.toArray();
   if (rowArr.size() != static_cast<int>(sgh) ||
       varr.size() != static_cast<int>(sgh))
   {
      return false;
   }

   QSplitter* const vs    = mapLayoutRoot_;
   const int        nRows = vs->count();
   if (nRows != static_cast<int>(sgh))
   {
      return false;
   }

   QList<int> vSizes;
   for (const auto& v : varr)
   {
      vSizes.append(v.toInt(1));
   }
   if (!map::MapPaneSplitterStateSizesAllPositive(vSizes))
   {
      return false;
   }

   QList<QList<int>> rowColSizes;
   for (int r = 0; r < rowArr.size(); ++r)
   {
      if (!rowArr.at(r).isArray())
      {
         return false;
      }
      auto* const hs = qobject_cast<QSplitter*>(vs->widget(r));
      if (hs == nullptr)
      {
         return false;
      }
      const QJsonArray cols = rowArr.at(r).toArray();
      if (hs->count() != cols.size() || cols.size() != static_cast<int>(sgw))
      {
         return false;
      }
      QList<int> cSizes;
      for (const auto& v : cols)
      {
         cSizes.append(v.toInt(1));
      }
      if (!map::MapPaneSplitterStateSizesAllPositive(cSizes))
      {
         return false;
      }
      rowColSizes.append(cSizes);
   }

   vs->setSizes(vSizes);
   for (int r = 0; r < rowColSizes.size(); ++r)
   {
      auto* const hs = qobject_cast<QSplitter*>(vs->widget(r));
      if (hs == nullptr)
      {
         return false;
      }
      hs->setSizes(rowColSizes.at(r));
   }

   return true;
}

void MainWindowImpl::SaveMapPaneSplitterState()
{
   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }

   QSplitter* const vs = mapLayoutRoot_;
   QJsonObject      root;
   root["gw"] = static_cast<double>(builtLayoutGridW_);
   root["gh"] = static_cast<double>(builtLayoutGridH_);

   QJsonArray vArr;
   for (const int s : vs->sizes())
   {
      vArr.append(s);
   }
   root["v"] = vArr;

   QJsonArray rowOuter;
   for (int r = 0; r < vs->count(); ++r)
   {
      auto* const hs = qobject_cast<QSplitter*>(vs->widget(r));
      if (hs == nullptr)
      {
         return;
      }
      QJsonArray cArr;
      for (const int s : hs->sizes())
      {
         cArr.append(s);
      }
      rowOuter.append(cArr);
   }
   root["rows"] = rowOuter;

   const QByteArray  bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
   const std::string json  = bytes.toStdString();
   settings::UiSettings::Instance().map_pane_splitter_state().StageValue(json);
}

bool MainWindowImpl::RestoreMapPaneViewLinkFromSettingsIfMatching()
{
   if (maps_.empty() || builtLayoutGridW_ < 1 || builtLayoutGridH_ < 1)
   {
      return false;
   }

   const std::string json =
      settings::UiSettings::Instance().map_pane_view_link_state().GetValue();
   if (json.empty())
   {
      return false;
   }

   const std::optional<std::vector<bool>> parsed =
      map::TryParseMapPaneViewLinkStateJson(
         json, builtLayoutGridW_, builtLayoutGridH_);
   if (!parsed.has_value() || parsed->size() != maps_.size())
   {
      return false;
   }

   SyncMapPaneViewLinkStateSize();
   if (mapPaneViewLinked_.size() != parsed->size())
   {
      return false;
   }

   for (std::size_t i = 0; i < parsed->size(); ++i)
   {
      mapPaneViewLinked_.at(i) = parsed->at(i);
   }

   return true;
}

void MainWindowImpl::SaveMapPaneViewLinkState()
{
   if (mapLayoutRoot_ == nullptr || maps_.empty() || builtLayoutGridW_ < 1 ||
       builtLayoutGridH_ < 1)
   {
      return;
   }

   SyncMapPaneViewLinkStateSize();

   std::vector<bool> v;
   v.reserve(mapPaneViewLinked_.size());
   for (std::size_t i = 0; i < mapPaneViewLinked_.size() && i < maps_.size();
        ++i)
   {
      v.push_back(mapPaneViewLinked_.at(i));
   }
   const std::string json = map::SerializeMapPaneViewLinkStateJson(
      builtLayoutGridW_, builtLayoutGridH_, v);
   if (json.empty())
   {
      return;
   }
   settings::UiSettings::Instance().map_pane_view_link_state().StageValue(json);
}

void MainWindowImpl::SetLinkRowSplitters(bool on)
{
   linkRowSplitters_ = on;
   if (on)
   {
      SnapLinkedColumnWidths();
   }
}

void MainWindowImpl::SetLinkColumnHeights(bool on)
{
   linkColumnHeights_ = on;
   if (on)
   {
      SnapLinkedColumnHeights();
   }
}

void MainWindowImpl::SnapLinkedColumnWidths()
{
   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }
   if (std::any_of(mapPanePoppedOut_.cbegin(),
                   mapPanePoppedOut_.cend(),
                   [](bool popped) { return popped; }))
   {
      ApplyEqualMapPaneSizes();
      return;
   }

   QList<int> referenceSizes;
   for (int r = 0; r < mapLayoutRoot_->count(); ++r)
   {
      auto* const hs = qobject_cast<QSplitter*>(mapLayoutRoot_->widget(r));
      if (hs == nullptr)
      {
         continue;
      }
      const QList<int> sizes = hs->sizes();
      int              total = 0;
      for (const int size : sizes)
      {
         total += size;
      }
      if (total > 0)
      {
         referenceSizes = sizes;
         break;
      }
   }
   if (referenceSizes.empty())
   {
      return;
   }
   for (int r = 0; r < mapLayoutRoot_->count(); ++r)
   {
      auto* const hs = qobject_cast<QSplitter*>(mapLayoutRoot_->widget(r));
      if (hs != nullptr && hs->count() == referenceSizes.size())
      {
         hs->setSizes(referenceSizes);
      }
   }
}

void MainWindowImpl::SnapLinkedColumnHeights()
{
   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }
   if (std::any_of(mapPanePoppedOut_.cbegin(),
                   mapPanePoppedOut_.cend(),
                   [](bool popped) { return popped; }))
   {
      ApplyEqualMapPaneSizes();
      return;
   }

   const int nRows = mapLayoutRoot_->count();
   if (nRows <= 0)
   {
      return;
   }
   const int  totalH = std::max(1, mapLayoutRoot_->height());
   const int  base   = totalH / nRows;
   int        acc    = 0;
   QList<int> sizes;
   for (int r = 0; r < nRows; ++r)
   {
      const int part = (r < nRows - 1) ? base : (totalH - acc);
      sizes.append(part);
      acc += part;
   }
   mapLayoutRoot_->setSizes(sizes);
}

void MainWindowImpl::ApplyMapGridPreset(int64_t gridWidth, int64_t gridHeight)
{
   auto& general = settings::GeneralSettings::Instance();
   general.grid_width().SetValue(gridWidth);
   general.grid_height().SetValue(gridHeight);
   RecreateMapLayoutFromUser();
}

void MainWindowImpl::UpdatePanesPresetSelection()
{
   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }

   auto* const                   ui            = mainWindow_->ui;
   const std::array<QAction*, 5> presetActions = {ui->actionPanes1x1,
                                                  ui->actionPanes1x2,
                                                  ui->actionPanes2x1,
                                                  ui->actionPanes2x2,
                                                  ui->actionPanes3x3};
   const int                     w = static_cast<int>(builtLayoutGridW_);
   const int                     h = static_cast<int>(builtLayoutGridH_);
   int                           matchIndex = -1;
   if (w == 1 && h == 1)
   {
      matchIndex = 0;
   }
   else if (w == 1 && h == 2)
   {
      matchIndex = 1;
   }
   else if (w == 2 && h == 1)
   {
      matchIndex = 2;
   }
   else if (w == 2 && h == 2)
   {
      matchIndex = 3;
   }
   else if (w == 3 && h == 3)
   {
      matchIndex = 4;
   }

   for (QAction* const a : presetActions)
   {
      const QSignalBlocker block {a};
      a->setChecked(false);
   }
   const auto nPresets = static_cast<int>(presetActions.size());
   if (matchIndex >= 0 && matchIndex < nPresets)
   {
      const QSignalBlocker block {
         presetActions.at(static_cast<std::size_t>(matchIndex))};
      presetActions.at(static_cast<std::size_t>(matchIndex))->setChecked(true);
   }
}

void MainWindowImpl::SetupPanesMenu()
{
   auto* const u = mainWindow_->ui;
   {
      const QSignalBlocker block {u->actionPanesLinkColumnWidth};
      u->actionPanesLinkColumnWidth->setChecked(linkRowSplitters_);
   }
   {
      const QSignalBlocker block {u->actionPanesLinkColumnHeight};
      u->actionPanesLinkColumnHeight->setChecked(linkColumnHeights_);
   }
   {
      const QSignalBlocker block {u->actionPanesMatchMapStyle};
      u->actionPanesMatchMapStyle->setChecked(settings::UiSettings::Instance()
                                                 .panes_match_map_style()
                                                 .GetStagedOrValue());
   }
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): parented to main window
   auto* presetGroup = new QActionGroup(mainWindow_);
   presetGroup->setExclusionPolicy(
      QActionGroup::ExclusionPolicy::ExclusiveOptional);
   presetGroup->addAction(u->actionPanes1x1);
   presetGroup->addAction(u->actionPanes1x2);
   presetGroup->addAction(u->actionPanes2x1);
   presetGroup->addAction(u->actionPanes2x2);
   presetGroup->addAction(u->actionPanes3x3);
   UpdatePanesPresetSelection();
}

void MainWindowImpl::ConfigureMapStyles(const bool mapStylesIgnoreLiveWidget)
{
   const auto& mapProviderInfo = map::GetMapProviderInfo(mapProvider_);
   auto&       mapSettings     = settings::MapSettings::Instance();

   const bool matchMapStyle = settings::UiSettings::Instance()
                                 .panes_match_map_style()
                                 .GetStagedOrValue();

   // Validate a name from settings / UI against the current provider.
   const auto validOrDefaultStyle = [&](std::string styleName) -> std::string
   {
      if (!((customStyleAvailable_ && styleName == "Custom") ||
            styleName == "None" ||
            std::ranges::find_if(mapProviderInfo.mapStyles_,
                                 [&](const auto& mapStyle)
                                 { return mapStyle.name_ == styleName; }) !=
               mapProviderInfo.mapStyles_.cend()))
      {
         styleName = !mapProviderInfo.mapStyles_.empty() ?
                        mapProviderInfo.mapStyles_.at(0).name_ :
                        "None";
      }
      return styleName;
   };

   // When matching, every MapWidget must get the *same* initialStyleName_;
   // otherwise each pane's initializeGL/ResetMap reapplies a different per-slot
   // name after launch (map_style(0) vs map_style(1)…), and panes look out of
   // sync until layout is rebuilt.
   std::string sharedMapStyle;
   if (matchMapStyle && !maps_.empty() && maps_.front() != nullptr)
   {
      std::string styleName =
         validOrDefaultStyle(mapSettings.map_style(0).GetStagedOrValue());
      if (!mapStylesIgnoreLiveWidget)
      {
         const std::string current = maps_.front()->GetMapStyle();
         if (current != "?")
         {
            styleName = current;
         }
      }
      sharedMapStyle = std::move(styleName);
   }

   for (std::size_t i = 0; i < maps_.size(); i++)
   {
      const std::string configuredStyleName =
         mapSettings.map_style(i).GetStagedOrValue();
      std::string styleName;

      if (matchMapStyle)
      {
         styleName = sharedMapStyle;
      }
      else
      {
         styleName = validOrDefaultStyle(configuredStyleName);

         const bool newPaneRespectsPerIndexSavedStyle =
            std::find(pendingPanesInheritFromRef_.cbegin(),
                      pendingPanesInheritFromRef_.cend(),
                      i) != pendingPanesInheritFromRef_.cend();
         const std::string currentStyleName = maps_.at(i)->GetMapStyle();
         if (!mapStylesIgnoreLiveWidget && !newPaneRespectsPerIndexSavedStyle &&
             currentStyleName != "?")
         {
            styleName = currentStyleName;
         }
      }

      maps_.at(i)->SetInitialMapStyle(styleName);
      // SetInitialMapStyle only stores the name; SetMapStyle applies to the
      // live map.
      maps_.at(i)->SetMapStyle(styleName, true);

      if (maps_[i] == activeMap_)
      {
         UpdateMapStyle(styleName);
      }

      // While match is on, never write resolved style back into map_style(i):
      // that would replace the user's per-slot saved preference (used when
      // match is off) with the live widget / validation result.
      if (!matchMapStyle && configuredStyleName != styleName)
      {
         mapSettings.map_style(i).StageValue(styleName);
      }
   }
}

void MainWindowImpl::StageMapIndexFromRefWidget(const std::size_t     mapIndex,
                                                const map::MapWidget& ref,
                                                const bool copyMapStyle)
{
   (void) copyMapStyle;
   auto& mapSettings = settings::MapSettings::Instance();
   // Do not StageValue map_style from the reference pane: each index keeps its
   // own MapSettings map_style (e.g. after 1x1 -> 3x3). Visual style for new
   // panes when match is on is SetInitialMapStyle in ApplyReferencePane.
   if (const auto site = ref.GetRadarSite())
   {
      mapSettings.radar_site(mapIndex).StageValue(site->id());
   }
   // Do not copy radar product group/product from the reference pane: each map
   // index keeps its own MapSettings defaults (e.g. distinct L3 products per
   // slot).
   mapSettings.smoothing_enabled(mapIndex).StageValue(
      ref.GetSmoothingEnabled());
}

void MainWindowImpl::ApplyReferencePaneToPendingNewPanes()
{
   if (maps_.empty() || maps_.front() == nullptr)
   {
      pendingPanesInheritFromRef_.clear();
      return;
   }

   map::MapWidget* const ref = maps_.front();
   // copyMapStyle: only whether we copy the *basemap* from pane 0. Radar
   // product / site / smoothing are handled separately; "Match map style" is
   // map style only.
   const bool copyMapStyle = settings::UiSettings::Instance()
                                .panes_match_map_style()
                                .GetStagedOrValue();
   for (const std::size_t i : pendingPanesInheritFromRef_)
   {
      if (i >= maps_.size() || maps_.at(i) == nullptr)
      {
         continue;
      }

      map::MapWidget* const w = maps_.at(i);
      if (const auto site = ref->GetRadarSite())
      {
         w->SelectRadarSite(site->id(), false);
      }
      {
         // Re-apply after SelectRadarSite; ApplyRadarProducts ran with ctor
         // default site.
         auto&             mapSettings = settings::MapSettings::Instance();
         const std::string pg0 =
            mapSettings.radar_product_group(0).GetStagedOrValue();
         const std::string p0 = mapSettings.radar_product(0).GetStagedOrValue();
         const std::string pgi =
            mapSettings.radar_product_group(i).GetStagedOrValue();
         const std::string pi = mapSettings.radar_product(i).GetStagedOrValue();
         // If this slot matches pane 0 in settings, use the factory per-index
         // default (kDefaultRadarProduct_); otherwise keep the slot's own saved
         // L3+group.
         const bool        useFactoryDefaults = (pgi == pg0 && pi == p0);
         const std::string groupName =
            useFactoryDefaults ?
               mapSettings.radar_product_group(i).GetDefault() :
               pgi;
         const std::string productName =
            useFactoryDefaults ? mapSettings.radar_product(i).GetDefault() : pi;
         SelectRadarProduct(
            w, common::GetRadarProductGroup(groupName), productName, 0);
         mapSettings.radar_product_group(i).StageValue(groupName);
         mapSettings.radar_product(i).StageValue(productName);
      }

      w->SetSmoothingEnabled(ref->GetSmoothingEnabled());

      if (copyMapStyle)
      {
         const std::string style = ref->GetMapStyle();
         if (style != "?")
         {
            w->SetInitialMapStyle(style);
         }
      }

      StageMapIndexFromRefWidget(i, *ref, copyMapStyle);
   }

   pendingPanesInheritFromRef_.clear();
}

void MainWindowImpl::UpdateMatchMapStyleFromPanesState(bool allowAutocheck)
{
   if (maps_.empty())
   {
      return;
   }
   auto& matchVar = settings::UiSettings::Instance().panes_match_map_style();
   const bool allSame = AllMapPanesShareSameMapStyle(maps_);
   if (allowAutocheck)
   {
      if (allSame && !matchVar.GetStagedOrValue())
      {
         matchVar.StageValue(true);
         const QSignalBlocker block {mainWindow_->ui->actionPanesMatchMapStyle};
         mainWindow_->ui->actionPanesMatchMapStyle->setChecked(true);
      }
   }
   else
   {
      if (!allSame && matchVar.GetStagedOrValue() &&
          AllMapPanesReportResolvedMapStyle(maps_))
      {
         matchVar.StageValue(false);
         const QSignalBlocker block {mainWindow_->ui->actionPanesMatchMapStyle};
         mainWindow_->ui->actionPanesMatchMapStyle->setChecked(false);
      }
   }
}

void MainWindowImpl::ApplyMatchMapStyleFromMainToAllPanes()
{
   if (maps_.empty() || maps_.front() == nullptr)
   {
      return;
   }
   // Visual sync only. Per-slot map_style(i) stays each pane's independent
   // saved preference; turning match off re-applies those (RestoreAllPanes...).
   std::string mainStyle = maps_.front()->GetMapStyle();
   if (mainStyle == "?")
   {
      mainStyle =
         settings::MapSettings::Instance().map_style(0).GetStagedOrValue();
   }
   if (mainStyle == "?" || mainStyle.empty())
   {
      return;
   }
   for (map::MapWidget* const w : maps_)
   {
      if (w == nullptr)
      {
         continue;
      }
      if (w->GetMapStyle() != mainStyle)
      {
         w->SetMapStyle(mainStyle, true);
      }
   }
   if (activeMap_ != nullptr)
   {
      UpdateMapStyle(activeMap_->GetMapStyle());
   }
}

void MainWindowImpl::RestoreAllPanesFromSavedMapSettings()
{
   if (maps_.empty())
   {
      return;
   }
   // "Match map style" is basemap only. Re-applying radar site + product from
   // MapSettings here fought live RadarProductManager and cleared the active
   // radar / product selection in the toolbox.
   ConfigureMapStyles(true);
   for (map::MapWidget* const w : maps_)
   {
      if (w != nullptr)
      {
         ApplyStoredColorTableThreshold(w);
      }
   }
   if (activeMap_ != nullptr)
   {
      UpdateRadarProductSettings();
      UpdateRadarSite();
      UpdateVcp();
   }
}

void MainWindowImpl::OnPanesMatchMapStyleToggled(bool checked)
{
   settings::UiSettings::Instance().panes_match_map_style().StageValue(checked);
   if (checked)
   {
      ApplyMatchMapStyleFromMainToAllPanes();
   }
   else
   {
      RestoreAllPanesFromSavedMapSettings();
   }
   UpdateMatchMapStyleFromPanesState(false);
}

void MainWindowImpl::ConfigureUiSettings()
{
   auto& uiSettings = settings::UiSettings::Instance();

   level2ProductsGroup_->SetExpanded(
      uiSettings.level2_products_expanded().GetValue());
   level2SettingsGroup_->SetExpanded(
      uiSettings.level2_settings_expanded().GetValue());
   level3ProductsGroup_->SetExpanded(
      uiSettings.level3_products_expanded().GetValue());
   level3SettingsGroup_->SetExpanded(
      uiSettings.level3_settings_expanded().GetValue());
   mapSettingsGroup_->SetExpanded(
      uiSettings.map_settings_expanded().GetValue());
   timelineGroup_->SetExpanded(uiSettings.timeline_expanded().GetValue());

   connect(level2ProductsGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.level2_products_expanded().StageValue(expanded); });
   connect(level2SettingsGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.level2_settings_expanded().StageValue(expanded); });
   connect(level3ProductsGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.level3_products_expanded().StageValue(expanded); });
   connect(level3SettingsGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.level3_settings_expanded().StageValue(expanded); });
   connect(mapSettingsGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.map_settings_expanded().StageValue(expanded); });
   connect(timelineGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.timeline_expanded().StageValue(expanded); });
}

void MainWindowImpl::ConnectMapSignals()
{
   for (const auto& mapWidget : maps_)
   {
      connect(mapWidget,
              &map::MapWidget::AlertSelected,
              alertDockWidget_,
              &ui::AlertDockWidget::SelectAlert);
      connect(mapWidget,
              &map::MapWidget::MapParametersChanged,
              this,
              &MainWindowImpl::UpdateMapParameters);
      connect(mapWidget,
              &map::MapWidget::MapPaneContextMenuRequested,
              this,
              &MainWindowImpl::OnMapPaneContextMenuRequested);
      connect(
         mapWidget,
         &map::MapWidget::MapParametersChanged,
         this,
         [this, mapWidget](double latitude, double longitude)
         {
            if (mapWidget == activeMap_)
            {
               Q_EMIT mainWindow_->ActiveMapMoved(latitude, longitude);
            }
         },
         Qt::QueuedConnection);

      connect(
         mapWidget,
         &map::MapWidget::MapStyleChanged,
         this,
         [this, mapWidget](const std::string& mapStyle)
         {
            const bool match = settings::UiSettings::Instance()
                                  .panes_match_map_style()
                                  .GetStagedOrValue();
            if (match)
            {
               for (map::MapWidget* w : maps_)
               {
                  if (w != nullptr)
                  {
                     w->SetMapStyle(mapStyle, true);
                  }
               }
               UpdateMapStyle(mapStyle);
            }
            else
            {
               for (std::size_t i = 0; i < maps_.size(); ++i)
               {
                  if (maps_.at(i) == mapWidget)
                  {
                     settings::MapSettings::Instance().map_style(i).StageValue(
                        mapStyle);
                     break;
                  }
               }
               if (mapWidget == activeMap_)
               {
                  UpdateMapStyle(mapStyle);
               }
            }
            UpdateMatchMapStyleFromPanesState(true);
         });

      connect(
         mapWidget,
         &map::MapWidget::MouseCoordinateChanged,
         this,
         [this](common::Coordinate coordinate)
         {
            const QString latitude = QString::fromStdString(
               common::GetLatitudeString(coordinate.latitude_));
            const QString longitude = QString::fromStdString(
               common::GetLongitudeString(coordinate.longitude_));

            coordinateLabel_->setText(
               QString("%1, %2").arg(latitude).arg(longitude));
            coordinateLabel_->setVisible(true);

            for (auto& map : maps_)
            {
               map->UpdateMouseCoordinate(coordinate);
            }
         },
         Qt::QueuedConnection);

      connect(
         mapWidget,
         &map::MapWidget::RadarSweepUpdated,
         this,
         [&]()
         {
            if (mapWidget == activeMap_)
            {
               UpdateRadarProductSelection(mapWidget->GetRadarProductGroup(),
                                           mapWidget->GetRadarProductName());
               UpdateRadarProductSettings();
               UpdateRadarSite();
               UpdateVcp();
            }
         },
         Qt::QueuedConnection);

      connect(mapWidget,
              &map::MapWidget::ScreenCaptureRequested,
              this,
              &MainWindowImpl::ScreenCapture);

      connect(
         mapWidget,
         &map::MapWidget::Level3ProductsChanged,
         this,
         [&]()
         {
            if (mapWidget == activeMap_)
            {
               UpdateAvailableLevel3Products();
            }
         },
         Qt::QueuedConnection);
      connect(
         mapWidget,
         &map::MapWidget::IncomingLevel2ElevationChanged,
         this,
         [this](std::optional<float>)
         { level2SettingsWidget_->UpdateSettings(activeMap_); },
         Qt::QueuedConnection);
   }
}

void MainWindowImpl::ConnectAnimationSignals()
{
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::DateTimeChanged,
           timelineManager_.get(),
           &manager::TimelineManager::SetDateTime);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::ViewTypeChanged,
           timelineManager_.get(),
           &manager::TimelineManager::SetViewType);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::LoopTimeChanged,
           timelineManager_.get(),
           &manager::TimelineManager::SetLoopTime);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::LoopSpeedChanged,
           timelineManager_.get(),
           &manager::TimelineManager::SetLoopSpeed);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::LoopDelayChanged,
           timelineManager_.get(),
           &manager::TimelineManager::SetLoopDelay);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::AnimationStepBeginSelected,
           timelineManager_.get(),
           &manager::TimelineManager::AnimationStepBegin);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::AnimationStepBackSelected,
           timelineManager_.get(),
           &manager::TimelineManager::AnimationStepBack);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::AnimationPlaySelected,
           timelineManager_.get(),
           &manager::TimelineManager::AnimationPlayPause);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::AnimationStepNextSelected,
           timelineManager_.get(),
           &manager::TimelineManager::AnimationStepNext);
   connect(animationDockWidget_,
           &ui::AnimationDockWidget::AnimationStepEndSelected,
           timelineManager_.get(),
           &manager::TimelineManager::AnimationStepEnd);

   connect(timelineManager_.get(),
           &manager::TimelineManager::SelectedTimeUpdated,
           [this](std::chrono::system_clock::time_point dateTime)
           {
              selectedTime_ = dateTime;

              for (auto map : maps_)
              {
                 map->SelectTime(dateTime);
                 textEventManager_->SelectTime(dateTime);
                 QMetaObject::invokeMethod(
                    map, static_cast<void (QWidget::*)()>(&QWidget::update));
              }
           });

   connect(timelineManager_.get(),
           &manager::TimelineManager::AnimationStateUpdated,
           animationDockWidget_,
           &ui::AnimationDockWidget::UpdateAnimationState);
   connect(timelineManager_.get(),
           &manager::TimelineManager::ViewTypeUpdated,
           animationDockWidget_,
           &ui::AnimationDockWidget::UpdateViewType);
   connect(timelineManager_.get(),
           &manager::TimelineManager::LiveStateUpdated,
           animationDockWidget_,
           &ui::AnimationDockWidget::UpdateLiveState);
   connect(timelineManager_.get(),
           &manager::TimelineManager::LiveStateUpdated,
           [this](bool isLive)
           {
              for (auto map : maps_)
              {
                 map->SetAutoUpdate(isLive);
              }
           });

   ConnectMapToTimelineAndRadarSiteSignals();
}

void MainWindowImpl::ConnectMapToTimelineAndRadarSiteSignals()
{
   for (std::size_t i = 0; i < maps_.size(); i++)
   {
      connect(maps_[i],
              &map::MapWidget::RadarSweepUpdated,
              timelineManager_.get(),
              [=, this]() { timelineManager_->ReceiveRadarSweepUpdated(i); });
      connect(maps_[i],
              &map::MapWidget::RadarSweepNotUpdated,
              timelineManager_.get(),
              [=, this](types::NoUpdateReason reason)
              { timelineManager_->ReceiveRadarSweepNotUpdated(i, reason); });
      connect(maps_[i],
              &map::MapWidget::WidgetPainted,
              timelineManager_.get(),
              [=, this]() { timelineManager_->ReceiveMapWidgetPainted(i); });
      connect(maps_[i],
              &map::MapWidget::RadarSiteRequested,
              this,
              [this](const std::string& id, bool updateCoordinates)
              {
                 auto* const source =
                    qobject_cast<map::MapWidget*>(QObject::sender());
                 if (source != nullptr)
                 {
                    SelectRadarSiteRespectingViewLink(
                       source, id, updateCoordinates);
                 }
                 UpdateRadarSite();
                 UpdateAvailableLevel3Products();
                 UpdateRadarProductSettings();
              });
   }
}

void MainWindowImpl::DisconnectMapDataConnections()
{
   for (map::MapWidget* map : maps_)
   {
      if (map == nullptr)
      {
         continue;
      }
      QObject::disconnect(map, nullptr, this, nullptr);
      QObject::disconnect(map, nullptr, alertDockWidget_, nullptr);
      QObject::disconnect(map, nullptr, timelineManager_.get(), nullptr);
   }
}

void MainWindowImpl::ReconnectMapDataConnections()
{
   DisconnectMapDataConnections();
   ConnectMapSignals();
   ConnectMapToTimelineAndRadarSiteSignals();
}

void MainWindowImpl::ConnectOtherSignals()
{
   connect(hotkeyManager_.get(),
           &manager::HotkeyManager::HotkeyPressed,
           mainWindow_,
           [this](types::Hotkey hotkey, bool /*isAutoRepeat*/)
           {
              if (hotkey == types::Hotkey::ToggleFullScreen)
              {
                 mainWindow_->ui->actionFullScreen->trigger();
              }
           });
   connect(qApp,
           &QApplication::focusChanged,
           mainWindow_,
           [this](QWidget* /*old*/, QWidget* now) { HandleFocusChange(now); });
   connect(
      mainWindow_->ui->mapStyleComboBox,
      &QComboBox::currentTextChanged,
      mainWindow_,
      [&](const QString& text)
      {
         const std::string s = text.toStdString();
         if (settings::UiSettings::Instance()
                .panes_match_map_style()
                .GetStagedOrValue())
         {
            for (map::MapWidget* w : maps_)
            {
               if (w != nullptr)
               {
                  w->SetMapStyle(s, true);
               }
            }
         }
         else
         {
            activeMap_->SetMapStyle(s);
            for (std::size_t i = 0; i < maps_.size(); ++i)
            {
               if (maps_[i] == activeMap_)
               {
                  settings::MapSettings::Instance().map_style(i).StageValue(s);
                  break;
               }
            }
         }
         UpdateMatchMapStyleFromPanesState(true);
      });
   connect(
      mainWindow_->ui->smoothRadarDataCheckBox,
      &QCheckBox::checkStateChanged,
      mainWindow_,
      [this](Qt::CheckState state)
      {
         const bool smoothingEnabled = (state == Qt::CheckState::Checked);

         auto it = std::find(maps_.cbegin(), maps_.cend(), activeMap_);
         if (it != maps_.cend())
         {
            const std::size_t i = std::distance(maps_.cbegin(), it);
            settings::MapSettings::Instance().smoothing_enabled(i).StageValue(
               smoothingEnabled);
         }

         // Turn on smoothing
         activeMap_->SetSmoothingEnabled(smoothingEnabled);
      });
   connect(mainWindow_->ui->trackLocationCheckBox,
           &QCheckBox::checkStateChanged,
           mainWindow_,
           [this](Qt::CheckState state)
           {
              bool trackingEnabled = (state == Qt::CheckState::Checked);

              settings::GeneralSettings::Instance().track_location().StageValue(
                 trackingEnabled);

              // Turn on location tracking
              positionManager_->TrackLocation(trackingEnabled);
           });
   connect(
      mainWindow_->ui->saveRadarProductsButton,
      &QAbstractButton::clicked,
      mainWindow_,
      [this]()
      {
         auto& mapSettings = settings::MapSettings::Instance();
         for (std::size_t i = 0; i < maps_.size(); i++)
         {
            const auto& map = maps_.at(i);
            mapSettings.radar_product_group(i).StageValue(
               common::GetRadarProductGroupName(map->GetRadarProductGroup()));
            mapSettings.radar_product(i).StageValue(map->GetRadarProductName());
         }
      });
   connect(
      level2ProductsWidget_,
      &ui::Level2ProductsWidget::RadarProductSelected,
      mainWindow_,
      [&](common::RadarProductGroup group,
          const std::string&        productName,
          int16_t                   productCode)
      { SelectRadarProduct(activeMap_, group, productName, productCode); });
   connect(
      level3ProductsWidget_,
      &ui::Level3ProductsWidget::RadarProductSelected,
      mainWindow_,
      [&](common::RadarProductGroup group,
          const std::string&        productName,
          int16_t                   productCode)
      { SelectRadarProduct(activeMap_, group, productName, productCode); });
   connect(level2SettingsWidget_,
           &ui::Level2SettingsWidget::ElevationSelected,
           mainWindow_,
           [&](float elevation) { SelectElevation(activeMap_, elevation); });
   connect(
      level2SettingsWidget_,
      &ui::Level2SettingsWidget::ThresholdChanged,
      mainWindow_,
      [&](std::optional<float> threshold)
      {
         if (activeMap_ != nullptr)
         {
            settings::ProductSettings::Instance().set_color_table_threshold(
               activeMap_->GetRadarProductGroup(),
               activeMap_->GetRadarProductName(),
               threshold);
            activeMap_->SetColorTableThreshold(threshold);
         }
      });
   connect(
      level3SettingsWidget_,
      &ui::Level3SettingsWidget::ThresholdChanged,
      mainWindow_,
      [&](std::optional<float> threshold)
      {
         if (activeMap_ != nullptr)
         {
            settings::ProductSettings::Instance().set_color_table_threshold(
               activeMap_->GetRadarProductGroup(),
               activeMap_->GetRadarProductName(),
               threshold);
            activeMap_->SetColorTableThreshold(threshold);
         }
      });
   connect(mainWindow_,
           &MainWindow::ActiveMapMoved,
           alertDockWidget_,
           &ui::AlertDockWidget::HandleMapUpdate,
           Qt::QueuedConnection);
   connect(
      alertDockWidget_,
      &ui::AlertDockWidget::MoveMap,
      this,
      [this](double latitude, double longitude)
      {
         for (map::MapWidget* map : maps_)
         {
            map->SetMapLocation(latitude, longitude, true);
         }

         UpdateRadarSite();
      },
      Qt::QueuedConnection);
   connect(mainWindow_,
           &MainWindow::ActiveMapMoved,
           radarSiteDialog_,
           &ui::RadarSiteDialog::HandleMapUpdate);
   connect(layerModel_.get(),
           &model::LayerModel::LayerDisplayChanged,
           this,
           [this](types::LayerInfo layer)
           {
              // Find matching layer action
              auto it =
                 std::find_if(layerActions_.begin(),
                              layerActions_.end(),
                              [&](const auto& layerAction)
                              {
                                 const auto& [type, description, action] =
                                    layerAction;
                                 return layer.type_ == type &&
                                        layer.description_ == description;
                              });

              // If matching layer action was found
              if (it != layerActions_.end())
              {
                 // Check the action if the layer is displayed on any map
                 bool anyDisplayed = std::find(layer.displayed_.begin(),
                                               layer.displayed_.end(),
                                               true) != layer.displayed_.end();

                 auto& action = std::get<2>(*it);
                 action->setChecked(anyDisplayed);
              }
           });
   connect(layerModel_.get(),
           &QAbstractItemModel::modelReset,
           this,
           [this]() { InitializeLayerDisplayActions(); });
   connect(
      radarSiteDialog_,
      &ui::RadarSiteDialog::accepted,
      this,
      [&]()
      {
         const std::string selectedRadarSite = radarSiteDialog_->radar_site();
         SelectRadarSiteRespectingViewLink(nullptr, selectedRadarSite, true);
         UpdateRadarSite();
         UpdateAvailableLevel3Products();
         UpdateRadarProductSettings();
      });
   connect(radarSiteModel_.get(),
           &model::RadarSiteModel::PresetToggled,
           [this](const std::string& siteId, bool isPreset)
           {
              if (isPreset && !radarSitePresetsActions_.contains(siteId))
              {
                 AddRadarSitePreset(siteId);
              }
              else if (!isPreset)
              {
                 auto entry = radarSitePresetsActions_.find(siteId);
                 if (entry != radarSitePresetsActions_.cend())
                 {
                    radarSitePresetsMenu_->removeAction(entry->second.get());
                    radarSitePresetsActions_.erase(entry);
                 }
              }

              mainWindow_->ui->radarSitePresetsButton->setVisible(
                 !radarSitePresetsActions_.empty());
           });
   connect(updateManager_.get(),
           &manager::UpdateManager::UpdateAvailable,
           this,
           [this](const std::string&        latestVersion,
                  const types::gh::Release& latestRelease)
           {
              updateDialog_->UpdateReleaseInfo(latestVersion, latestRelease);
              updateDialog_->show();
           });

   connect(&clockTimer_,
           &QTimer::timeout,
           this,
           [this]()
           {
              timeLabel_->setText(
                 QString::fromStdString(util::TimeString(util::time::now())));
              timeLabel_->setVisible(true);
           });
   clockTimer_.start(1000);

   auto& generalSettings = settings::GeneralSettings::Instance();
   homeRadarConnection_ =
      generalSettings.default_radar_site().changed_signal().connect(
         [this](const auto& event)
         {
            const std::shared_ptr<config::RadarSite> radarSite =
               activeMap_->GetRadarSite();
            const std::string& homeRadarSite = event.newValue_;
            if (radarSite == nullptr)
            {
               mainWindow_->ui->saveRadarProductsButton->setVisible(false);
            }
            else
            {
               mainWindow_->ui->saveRadarProductsButton->setVisible(
                  radarSite->id() == homeRadarSite);
            }
         });

   clockFormatConnection_ =
      generalSettings.clock_format().changed_signal().connect(
         [](const auto& event)
         {
            util::time::set_default_clock_format(
               util::GetClockFormat(event.newValue_));
         });
   defaultTimeZoneConnection_ =
      generalSettings.default_time_zone().changed_signal().connect(
         [this](auto&&...)
         {
            const auto defaultTimeZone = activeMap_->GetDefaultTimeZone();
            util::time::set_current_time_zone(defaultTimeZone);
            animationDockWidget_->UpdateTimeZone(defaultTimeZone);
         });

   connections_.emplace_back(
      generalSettings.custom_style_url().changed_signal().connect(
         [this](auto&&...) { PopulateCustomMapStyle(); }));
   connections_.emplace_back(
      generalSettings.custom_style_draw_layer().changed_signal().connect(
         [this](auto&&...) { PopulateCustomMapStyle(); }));

   connections_.emplace_back(
      generalSettings.map_provider().changed_signal().connect(
         [this](const auto& event)
         {
            mapProvider_ = map::GetMapProvider(event.newValue_);
            PopulateMapStyles();
            ConfigureMapStyles();
            if (settings::UiSettings::Instance()
                   .panes_match_map_style()
                   .GetStagedOrValue())
            {
               ApplyMatchMapStyleFromMainToAllPanes();
            }
            UpdateMatchMapStyleFromPanesState(false);
         }));

   connections_.emplace_back(
      generalSettings.grid_width().changed_signal().connect(
         [this](const auto& /*event*/)
         { ScheduleMapLayoutSyncIfGridChanged(); }));
   connections_.emplace_back(
      generalSettings.grid_height().changed_signal().connect(
         [this](const auto& /*event*/)
         { ScheduleMapLayoutSyncIfGridChanged(); }));

   // Ensure default clock format is initialized
   util::time::set_default_clock_format(
      util::GetClockFormat(generalSettings.clock_format().GetValue()));
}

void MainWindowImpl::InitializeLayerDisplayActions()
{
   if (!layerActionsInitialized_)
   {
      layerActions_.emplace(types::LayerType::Information,
                            types::InformationLayer::ColorTable,
                            mainWindow_->ui->actionColorTable);
      layerActions_.emplace(types::LayerType::Information,
                            types::InformationLayer::RadarSite,
                            mainWindow_->ui->actionRadarSites);
      layerActions_.emplace(types::LayerType::Data,
                            types::DataLayer::RadarRange,
                            mainWindow_->ui->actionRadarRange);
      layerActionsInitialized_ = true;
   }

   for (auto& layerAction : layerActions_)
   {
      auto& [type, description, action] = layerAction;

      types::LayerInfo layer = layerModel_->GetLayerInfo(type, description);

      bool anyDisplayed =
         std::find(layer.displayed_.begin(), layer.displayed_.end(), true) !=
         layer.displayed_.end();

      action->setChecked(anyDisplayed);
   }
}

void MainWindowImpl::AddRadarSitePreset(const std::string& siteId)
{
   auto        radarSite = config::RadarSite::Get(siteId);
   std::string actionText =
      fmt::format("{}: {}", siteId, radarSite->location_name());

   auto pair = radarSitePresetsActions_.emplace(
      siteId, std::make_shared<QAction>(QString::fromStdString(actionText)));
   auto& action = pair.first->second;

   QAction* before = nullptr;

   // If the radar site is not at the end
   if (pair.first != std::prev(radarSitePresetsActions_.cend()))
   {
      // Insert before the next entry in the list
      before = std::next(pair.first)->second.get();
   }

   radarSitePresetsMenu_->insertAction(before, action.get());

   connect(action.get(),
           &QAction::triggered,
           [this, siteId]()
           {
              SelectRadarSiteRespectingViewLink(nullptr, siteId, true);
              UpdateRadarSite();
              UpdateAvailableLevel3Products();
              UpdateRadarProductSettings();
           });
}

void MainWindowImpl::HandleFocusChange(QWidget* focused)
{
   map::MapWidget* mapWidget = dynamic_cast<map::MapWidget*>(focused);

   if (mapWidget != nullptr)
   {
      SetActiveMap(mapWidget);
      UpdateAvailableLevel3Products();
      UpdateMapStyle(mapWidget->GetMapStyle());
      UpdateRadarProductSelection(mapWidget->GetRadarProductGroup(),
                                  mapWidget->GetRadarProductName());
      UpdateRadarProductSettings();
      UpdateRadarSite();
      UpdateVcp();
   }
}

void MainWindowImpl::PopulateCustomMapStyle()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   auto customStyleUrl = generalSettings.custom_style_url().GetValue();
   auto customStyleDrawLayer =
      generalSettings.custom_style_draw_layer().GetValue();

   bool newCustomStyleAvailable =
      !customStyleUrl.empty() && !customStyleDrawLayer.empty();

   if (newCustomStyleAvailable != customStyleAvailable_)
   {
      static const QString kCustom {"Custom"};

      if (newCustomStyleAvailable)
      {

         mainWindow_->ui->mapStyleComboBox->addItem(kCustom);
      }
      else
      {
         int index = mainWindow_->ui->mapStyleComboBox->findText(kCustom);
         mainWindow_->ui->mapStyleComboBox->removeItem(index);
      }

      customStyleAvailable_ = newCustomStyleAvailable;
   }
}

void MainWindowImpl::PopulateMapStyles()
{
   const QSignalBlocker blocker(mainWindow_->ui->mapStyleComboBox);

   mainWindow_->ui->mapStyleComboBox->clear();

   const auto& mapProviderInfo = map::GetMapProviderInfo(mapProvider_);
   for (const auto& mapStyle : mapProviderInfo.mapStyles_)
   {
      mainWindow_->ui->mapStyleComboBox->addItem(
         QString::fromStdString(mapStyle.name_));
   }

   const std::string kNone = "None";
   mainWindow_->ui->mapStyleComboBox->addItem(QString::fromStdString(kNone));

   // The combobox was cleared above, so force re-evaluation of custom style.
   customStyleAvailable_ = false;

   PopulateCustomMapStyle();
}

void MainWindowImpl::ScreenCapture(types::CaptureType captureType)
{
   for (auto& map : maps_)
   {
      if (map == activeMap_ || captureType == types::CaptureType::SaveImage)
      {
         map->ScreenCapture(captureType);
      }
   }
}

void MainWindowImpl::SelectElevation(map::MapWidget* mapWidget, float elevation)
{
   if (mapWidget == activeMap_)
   {
      UpdateElevationSelection(elevation);
   }

   mapWidget->SelectElevation(elevation);
}

void MainWindowImpl::SelectRadarProduct(map::MapWidget*           mapWidget,
                                        common::RadarProductGroup group,
                                        const std::string&        productName,
                                        int16_t                   productCode)
{
   logger_->debug("Selecting radar product: {}, {}",
                  common::GetRadarProductGroupName(group),
                  productName);

   if (mapWidget == activeMap_)
   {
      UpdateRadarProductSelection(group, productName);
      UpdateRadarProductSettings();
   }

   mapWidget->SelectRadarProduct(
      group, productName, productCode, selectedTime_);
   ApplyStoredColorTableThreshold(mapWidget);

   if (mapWidget == activeMap_)
   {
      UpdateRadarProductSettings();
   }
}

void MainWindowImpl::ApplyStoredColorTableThreshold(map::MapWidget* mapWidget)
{
   if (mapWidget == nullptr)
   {
      return;
   }

   const auto threshold =
      settings::ProductSettings::Instance().color_table_threshold(
         mapWidget->GetRadarProductGroup(), mapWidget->GetRadarProductName());

   if (mapWidget->GetColorTableThreshold() == threshold)
   {
      return;
   }

   mapWidget->SetColorTableThreshold(threshold);
}

void MainWindowImpl::SetActiveMap(map::MapWidget* mapWidget)
{
   if (mapWidget != nullptr &&
       std::find(maps_.cbegin(), maps_.cend(), mapWidget) == maps_.cend())
   {
      mapWidget = nullptr;
   }

   if (mapWidget == activeMap_)
   {
      return;
   }

   activeMap_ = mapWidget;

   for (map::MapWidget* widget : maps_)
   {
      widget->SetActive(mapWidget == widget);
   }

   if (!mapAnnotationDock_.isNull())
   {
      if (activeMap_ != nullptr)
      {
         mapAnnotationDock_->AttachToMap(activeMap_);
         mapAnnotationDock_->BindToLayer(activeMap_->map_annotation_layer(),
                                         false);
      }
      else
      {
         mapAnnotationDock_->AttachToMap(nullptr);
         mapAnnotationDock_->BindToLayer(nullptr);
      }
   }
}

void MainWindowImpl::UpdateAvailableLevel3Products()
{
   level3ProductsWidget_->UpdateAvailableProducts(
      activeMap_->GetAvailableLevel3Categories());
}

void MainWindowImpl::UpdateElevationSelection(float elevation)
{
   level2SettingsWidget_->UpdateElevationSelection(elevation);
}

void MainWindowImpl::HandleMapPaneLinkViewToggled(std::size_t     mapIndex,
                                                  map::MapWidget* map,
                                                  bool            linked)
{
   SyncMapPaneViewLinkStateSize();
   if (mapIndex >= mapPaneViewLinked_.size())
   {
      return;
   }
   mapPaneViewLinked_[mapIndex] = linked;
   SaveMapPaneViewLinkState();
   if (linked)
   {
      // If active is this pane, copying from activeMap_ is a
      // no-op (SetMapParameters skips when view unchanged) — pick
      // another linked pane, or any other map, so relink applies
      // immediately.
      map::MapWidget* ref = nullptr;
      if (activeMap_ != nullptr && activeMap_ != map)
      {
         ref = activeMap_;
      }
      if (ref == nullptr)
      {
         for (std::size_t j = 0; j < maps_.size(); ++j)
         {
            if (j == mapIndex)
            {
               continue;
            }
            if (j < mapPaneViewLinked_.size() && mapPaneViewLinked_.at(j))
            {
               ref = maps_.at(j);
               break;
            }
         }
      }
      if (ref == nullptr)
      {
         for (map::MapWidget* m : maps_)
         {
            if (m != map)
            {
               ref = m;
               break;
            }
         }
      }
      if (ref != nullptr)
      {
         double lat {}, lon {}, zoom {}, bearing {}, pitch {};
         ref->GetMapViewParameters(lat, lon, zoom, bearing, pitch);
         map->SetMapParameters(lat, lon, zoom, bearing, pitch);
         if (const std::shared_ptr<config::RadarSite> site =
                ref->GetRadarSite();
             site != nullptr)
         {
            // Match linked panes' site; skip center jump — view
            // already snapped from |ref| above.
            map->SelectRadarSite(site->id(), false);
         }
      }
   }
}

void MainWindowImpl::OnMapPaneContextMenuRequested(const QPoint& globalPos)
{
   auto* const map = qobject_cast<map::MapWidget*>(sender());
   if (map == nullptr)
   {
      return;
   }

   SyncMapPaneViewLinkStateSize();

   const auto it = std::ranges::find(maps_, map);
   if (it == maps_.end())
   {
      return;
   }
   const auto mapIndex =
      static_cast<std::size_t>(std::distance(maps_.begin(), it));

   map::MapPaneContextMenuConfig cfg;
   cfg.event_receiver       = this;
   cfg.menu_parent          = map->window();
   cfg.title_map            = mainWindow_->tr("Map");
   cfg.text_popout          = mainWindow_->tr("Pop-&out");
   cfg.text_dock            = mainWindow_->tr("&Dock");
   cfg.text_link            = mainWindow_->tr("Link view");
   cfg.tooltip_link_enabled = mainWindow_->tr(
      "When on, this pane matches the other linked panes' map "
      "view and "
      "radar site. Turn off to move and pick sites on your own; "
      "turn on to "
      "snap view (and the same site when relinking) to another "
      "map.");
   cfg.tooltip_link_disabled =
      mainWindow_->tr("Add more map panes in the Panes menu to use this.");
   cfg.text_reset_layout = mainWindow_->ui->actionRecreateMapLayout->text();
   cfg.tooltip_reset_layout_when_popped = mainWindow_->tr(
      "Use Dock on a popped-out map, or close this menu "
      "and reset layout from the main window.");
   cfg.text_draw            = mainWindow_->tr("&Draw");
   cfg.is_draw_toolbar_open = [this](std::size_t i)
   {
      if (mapAnnotationDock_.isNull() || i >= maps_.size() ||
          maps_.at(i) == nullptr)
      {
         return false;
      }
      return maps_.at(i) == activeMap_ && mapAnnotationDock_->PanelExpanded();
   };
   cfg.set_draw_toolbar_open = [this](std::size_t i, bool open)
   {
      if (i >= maps_.size() || maps_.at(i) == nullptr)
      {
         return;
      }
      map::MapWidget* const mw = maps_.at(i);
      if (mapAnnotationDock_.isNull())
      {
         // NOLINTBEGIN(cppcoreguidelines-owning-memory): Qt parent owns dock
         mapAnnotationDock_ =
            new ui::MapAnnotationDockWidget(mainWindow_->ui->centralwidget);
         // NOLINTEND(cppcoreguidelines-owning-memory)
         ConfigureMapAnnotationDock();
      }
      SetActiveMap(mw);
      if (!open)
      {
         mapAnnotationDock_->SetPanelExpanded(false);
         return;
      }
      // SetActiveMap no-ops when |mw| is already active; still re-attach after
      // pop-out/dock so placement and layer bind match the grid map again.
      mapAnnotationDock_->AttachToMap(mw);
      mapAnnotationDock_->BindToLayer(mw->map_annotation_layer(), false);
      mapAnnotationDock_->SetPanelExpanded(true);
   };
   cfg.map_index   = mapIndex;
   cfg.maps        = &maps_;
   cfg.view_linked = &mapPaneViewLinked_;
   cfg.popped_out  = &mapPanePoppedOut_;
   cfg.current_map = map;
   cfg.on_popout   = [this](std::size_t i)
   {
      PopOutMap(i);
   };
   cfg.on_dock = [this](std::size_t i)
   {
      DockPoppedMap(i);
   };
   cfg.on_link_toggled = [this](std::size_t i, map::MapWidget* w, bool l)
   {
      HandleMapPaneLinkViewToggled(i, w, l);
   };
   cfg.on_reset_layout = [this]()
   {
      RecreateMapLayoutFromUser(false);
   };
   cfg.append_radar_submenus = [this](QMenu& m, map::MapWidget* mw)
   {
      map::AppendMapPaneRadarContextMenu(
         m,
         mw,
         this,
         [this](map::MapWidget*           w,
                common::RadarProductGroup g,
                const std::string&        n,
                int16_t c) { SelectRadarProduct(w, g, n, c); },
         [this](const char* s) { return mainWindow_->tr(s); });
   };

   map::RunMapPaneContextMenu(cfg, globalPos);
}

void MainWindowImpl::UpdateMapParameters(
   double latitude, double longitude, double zoom, double bearing, double pitch)
{
   // Only the map that emitted already has this view; all other panes follow
   // (including the focused one when a non-focused pane moves the view).
   auto* const sourceMap = qobject_cast<map::MapWidget*>(QObject::sender());

   if (!map::ShouldApplyLinkedMapParameterSync(maps_.size(), sourceMap))
   {
      return;
   }

   SyncMapPaneViewLinkStateSize();

   if (sourceMap != nullptr)
   {
      const auto sit = std::ranges::find(maps_, sourceMap);
      if (sit != maps_.end())
      {
         const auto sourceIndex =
            static_cast<std::size_t>(std::distance(maps_.begin(), sit));
         if (sourceIndex < mapPaneViewLinked_.size() &&
             !mapPaneViewLinked_[sourceIndex])
         {
            return;
         }
      }
   }

   for (std::size_t i = 0; i < maps_.size(); ++i)
   {
      map::MapWidget* map = maps_.at(i);
      if (sourceMap != nullptr && map == sourceMap)
      {
         continue;
      }
      if (i < mapPaneViewLinked_.size() && !mapPaneViewLinked_.at(i))
      {
         continue;
      }
      map->SetMapParameters(latitude, longitude, zoom, bearing, pitch);
   }
}

void MainWindowImpl::UpdateMapStyle(const std::string& styleName)
{
   int index = mainWindow_->ui->mapStyleComboBox->findText(
      QString::fromStdString(styleName));
   if (index == -1)
   {
      return;
   }
   {
      const QSignalBlocker blocker(mainWindow_->ui->mapStyleComboBox);
      mainWindow_->ui->mapStyleComboBox->setCurrentIndex(index);
   }
   if (settings::UiSettings::Instance()
          .panes_match_map_style()
          .GetStagedOrValue())
   {
      // Do not write map_style(i); match mode is a shared display override
      // only.
      return;
   }
   for (std::size_t i = 0; i < maps_.size(); ++i)
   {
      if (maps_[i] == activeMap_)
      {
         settings::MapSettings::Instance().map_style(i).StageValue(styleName);
         break;
      }
   }
}

void MainWindowImpl::UpdateRadarProductSelection(
   common::RadarProductGroup group, const std::string& product)
{
   level2ProductsWidget_->UpdateProductSelection(group, product);
   level3ProductsWidget_->UpdateProductSelection(group, product);
}

void MainWindowImpl::UpdateRadarProductSettings()
{
   ApplyStoredColorTableThreshold(activeMap_);

   if (activeMap_->GetRadarProductGroup() == common::RadarProductGroup::Level2)
   {
      level2SettingsWidget_->setEnabled(true);
      level2SettingsGroup_->setVisible(true);
      // This should be done after setting visible for correct sizing
      level2SettingsWidget_->UpdateSettings(activeMap_);

      level3SettingsGroup_->setVisible(false);
      level3SettingsWidget_->setEnabled(false);
   }
   else if (activeMap_->GetRadarProductGroup() ==
            common::RadarProductGroup::Level3)
   {
      level2SettingsGroup_->setVisible(false);
      level2SettingsWidget_->setEnabled(false);

      level3SettingsWidget_->setEnabled(true);
      level3SettingsGroup_->setVisible(true);
      const bool hasContent =
         level3SettingsWidget_->UpdateThreshold(activeMap_);
      level3SettingsWidget_->setEnabled(hasContent);
      level3SettingsGroup_->setVisible(hasContent);
   }
   else
   {
      level2SettingsGroup_->setVisible(false);
      level2SettingsWidget_->setEnabled(false);

      level3SettingsGroup_->setVisible(false);
      level3SettingsWidget_->setEnabled(false);
   }

   mainWindow_->ui->smoothRadarDataCheckBox->setCheckState(
      activeMap_->GetSmoothingEnabled() ? Qt::CheckState::Checked :
                                          Qt::CheckState::Unchecked);

   mainWindow_->ui->actionRadarWireframe->setChecked(
      activeMap_->GetRadarWireframeEnabled());
}

void MainWindowImpl::UpdateRadarSite()
{
   std::shared_ptr<config::RadarSite> radarSite = activeMap_->GetRadarSite();
   const std::string homeRadarSite              = common::GetCanonicalRadarId(
      settings::GeneralSettings::Instance().default_radar_site().GetValue());

   if (radarSite != nullptr)
   {
      mainWindow_->setWindowTitle(
         tr("Supercell Wx - %1").arg(QString::fromStdString(radarSite->id())));

      mainWindow_->ui->radarSiteValueLabel->setVisible(true);
      mainWindow_->ui->radarLocationLabel->setVisible(true);

      mainWindow_->ui->radarSiteValueLabel->setText(radarSite->id().c_str());
      mainWindow_->ui->radarLocationLabel->setText(
         radarSite->location_name().c_str());

      timelineManager_->SetRadarSite(radarSite->id());

      mainWindow_->ui->saveRadarProductsButton->setVisible(radarSite->id() ==
                                                           homeRadarSite);
   }
   else
   {
      mainWindow_->setWindowTitle(tr("Supercell Wx"));

      mainWindow_->ui->radarSiteValueLabel->setVisible(false);
      mainWindow_->ui->radarLocationLabel->setVisible(false);
      mainWindow_->ui->saveRadarProductsButton->setVisible(false);

      timelineManager_->SetRadarSite("");
   }

   alertManager_->SetRadarSite(radarSite);
   placefileManager_->SetRadarSite(radarSite);

   const auto timeZone = activeMap_->GetDefaultTimeZone();
   util::time::set_current_time_zone(timeZone);
   animationDockWidget_->UpdateTimeZone(timeZone);
}

void MainWindowImpl::UpdateVcp()
{
   uint16_t vcp = activeMap_->GetVcp();

   if (vcp != 0)
   {
      mainWindow_->ui->vcpLabel->setVisible(true);
      mainWindow_->ui->vcpValueLabel->setVisible(true);
      mainWindow_->ui->vcpDescriptionLabel->setVisible(true);

      mainWindow_->ui->vcpValueLabel->setText(QString::number(vcp));
      mainWindow_->ui->vcpDescriptionLabel->setText(
         tr(common::GetVcpDescription(vcp).c_str()));
   }
   else
   {
      mainWindow_->ui->vcpLabel->setVisible(false);
      mainWindow_->ui->vcpValueLabel->setVisible(false);
      mainWindow_->ui->vcpDescriptionLabel->setVisible(false);
   }
}

void MainWindowImpl::PopOutMap(std::size_t mapIndex)
{
   if (mapIndex >= maps_.size() || mapLayoutRoot_ == nullptr)
   {
      return;
   }
   SyncMapPaneViewLinkStateSize();
   if (mapIndex >= mapPanePoppedOut_.size() || mapPanePoppedOut_.at(mapIndex))
   {
      return;
   }

   map::MapWidget* map = maps_.at(mapIndex);
   QSplitter*      hs  = nullptr;
   int             idx = 0;
   if (!FindWidgetInGridSplitters(mapLayoutRoot_, map, &hs, &idx))
   {
      return;
   }

   // Parented into splitter via replaceWidget; Qt owns lifetime
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
   auto* ph = new QWidget();
   ph->setMinimumSize(0, 0);
   ph->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

   QWidget* const oldW = hs->replaceWidget(idx, ph);
   if (oldW != map)
   {
      if (oldW != nullptr)
      {
         // Caller owns the replaced widget; not parented to splitter
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         delete oldW;
      }
      return;
   }

   if (!mapAnnotationDock_.isNull())
   {
      mapAnnotationDock_->DetachIfHostedBy(map, true);
      if (map == activeMap_)
      {
         mapAnnotationDock_->SetPanelExpanded(false);
      }
   }

   map->setParent(nullptr);
   ph->show();

   mapPanePlaceholders_.at(mapIndex) = ph;

   auto                 frame = std::make_unique<map::MapPopoutFrame>(mapIndex);
   map::MapPopoutFrame* fr    = frame.get();
   const QRect          fgeo  = mainWindow_->frameGeometry();
   const int defW = std::min(800, std::max(400, fgeo.width() * 2 / 3));
   const int defH = std::min(600, std::max(300, fgeo.height() * 2 / 3));
   fr->resize(defW, defH);
   fr->move(fgeo.x() + std::max(0, fgeo.width() - defW) / 2,
            fgeo.y() + std::max(0, fgeo.height() - defH) / 3);
   fr->SetEmbeddedMap(map);
   connect(
      fr,
      &map::MapPopoutFrame::DockMapRequested,
      this,
      [this, mapIndex]() { DockPoppedMap(mapIndex); },
      Qt::QueuedConnection);
   fr->show();
   map->setFocus();
   mapPanePoppedOut_.at(mapIndex)    = true;
   mapPanePopoutFrames_.at(mapIndex) = std::move(frame);
   ApplyEqualMapPaneSizes();
   ScheduleMapPaneGeometryApply();
}

void MainWindowImpl::DockPoppedMap(std::size_t mapIndex)
{
   if (mapIndex >= maps_.size())
   {
      return;
   }
   SyncMapPaneViewLinkStateSize();
   if (mapIndex >= mapPanePoppedOut_.size() || !mapPanePoppedOut_.at(mapIndex))
   {
      return;
   }

   map::MapWidget* const map = maps_.at(mapIndex);
   if (map == nullptr)
   {
      return;
   }

   // Stale: marked popped, but the map is already in the main splitter grid.
   if (mapLayoutRoot_ != nullptr)
   {
      QSplitter* alreadyHs  = nullptr;
      int        alreadyIdx = 0;
      if (FindWidgetInGridSplitters(
             mapLayoutRoot_, map, &alreadyHs, &alreadyIdx))
      {
         mapPanePoppedOut_.at(mapIndex) = false;
         const QPointer<QWidget> phRef  = mapPanePlaceholders_.at(mapIndex);
         if (!phRef.isNull())
         {
            QSplitter* phHs = nullptr;
            int        phI  = 0;
            if (FindWidgetInGridSplitters(
                   mapLayoutRoot_, phRef.data(), &phHs, &phI))
            {
               phRef->deleteLater();
            }
         }
         mapPanePlaceholders_.at(mapIndex) = nullptr;
         mapPanePopoutFrames_.at(mapIndex).reset();
         if (!mapAnnotationDock_.isNull())
         {
            mapAnnotationDock_->DetachIfHostedBy(map, true);
            if (map == activeMap_)
            {
               mapAnnotationDock_->SetPanelExpanded(false);
            }
         }
         return;
      }
   }

   if (!mapPanePopoutFrames_.at(mapIndex) &&
       mapPanePlaceholders_.at(mapIndex).isNull())
   {
      mapPanePoppedOut_.at(mapIndex) = false;
      return;
   }

   QWidget* ph = mapPanePlaceholders_.at(mapIndex).data();
   if (ph == nullptr)
   {
      // No grid placeholder — cannot reinsert. Leave the map in the popout.
      return;
   }

   if (mapLayoutRoot_ == nullptr)
   {
      return;
   }

   QSplitter* layoutHs  = nullptr;
   int        layoutIdx = 0;
   if (!FindWidgetInGridSplitters(mapLayoutRoot_, ph, &layoutHs, &layoutIdx))
   {
      if (builtLayoutGridW_ < 1 ||
          !MapSlotByMapIndex(mapLayoutRoot_,
                             builtLayoutGridW_,
                             mapIndex,
                             &layoutHs,
                             &layoutIdx) ||
          layoutHs->widget(layoutIdx) != ph)
      {
         // Placeholder is not the grid cell for this index — do not detach the
         // map.
         return;
      }
   }

   if (mapPanePopoutFrames_.at(mapIndex))
   {
      if (!mapAnnotationDock_.isNull())
      {
         mapAnnotationDock_->DetachIfHostedBy(map, true);
      }
      mapPanePopoutFrames_.at(mapIndex)->DetachMapWidget();
      mapPanePopoutFrames_.at(mapIndex).reset();
   }

   layoutHs->setVisible(true);
   ph->setVisible(true);
   QWidget* const removed = layoutHs->replaceWidget(layoutIdx, map);
   // Caller owns the replaced placeholder widget
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
   delete removed;
   if (QWidget* rowWidget =
          mapLayoutRoot_->widget(mapLayoutRoot_->indexOf(layoutHs));
       rowWidget != nullptr)
   {
      rowWidget->setVisible(true);
      mapLayoutRoot_->setCollapsible(mapLayoutRoot_->indexOf(layoutHs), false);
      mapLayoutRoot_->setStretchFactor(mapLayoutRoot_->indexOf(layoutHs), 1);
   }
   layoutHs->setCollapsible(layoutIdx, false);
   layoutHs->setStretchFactor(layoutIdx, 1);
   map->show();
   map->raise();
   mapPanePlaceholders_.at(mapIndex) = nullptr;
   mapPanePoppedOut_.at(mapIndex)    = false;
   if (!mapAnnotationDock_.isNull() && map == activeMap_)
   {
      mapAnnotationDock_->SetPanelExpanded(false);
   }
   ApplyEqualMapPaneSizes();
   ScheduleMapPaneGeometryApply();
}

void MainWindowImpl::DockAllPoppedBeforeTeardown()
{
   for (std::size_t i = 0; i < mapPanePoppedOut_.size(); ++i)
   {
      if (mapPanePoppedOut_.at(i))
      {
         DockPoppedMap(i);
      }
   }
}

void MainWindowImpl::SavePoppedMapWindowsToSettings()
{
   QJsonObject root;
   root["gw"] = static_cast<double>(builtLayoutGridW_);
   root["gh"] = static_cast<double>(builtLayoutGridH_);
   QJsonArray panes;
   for (std::size_t i = 0; i < maps_.size() && i < mapPanePoppedOut_.size();
        ++i)
   {
      if (!mapPanePoppedOut_.at(i) || !mapPanePopoutFrames_.at(i))
      {
         continue;
      }
      QJsonObject o;
      o["i"] = static_cast<qint64>(i);
      const QByteArray geomB64 =
         mapPanePopoutFrames_.at(i)->saveGeometry().toBase64();
      o["g"] = QString::fromUtf8(geomB64);
      panes.append(o);
   }
   root["panes"]          = panes;
   const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
   settings::UiSettings::Instance().map_pane_popout_state().StageValue(
      bytes.toStdString());
}

void MainWindowImpl::TryRestorePoppedMapWindows()
{
   if (popoutPanesRestoredThisSession_)
   {
      return;
   }
   if (mapLayoutRoot_ == nullptr)
   {
      popoutPanesRestoredThisSession_ = true;
      return;
   }
   const std::string json =
      settings::UiSettings::Instance().map_pane_popout_state().GetValue();
   if (json.empty())
   {
      popoutPanesRestoredThisSession_ = true;
      return;
   }
   QJsonParseError     err {};
   const QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(json), &err);
   if (err.error != QJsonParseError::NoError || !doc.isObject())
   {
      popoutPanesRestoredThisSession_ = true;
      return;
   }
   const QJsonObject o   = doc.object();
   const int64_t     jgw = static_cast<int64_t>(o.value("gw").toDouble(0.0));
   const int64_t     jgh = static_cast<int64_t>(o.value("gh").toDouble(0.0));
   if (jgw != builtLayoutGridW_ || jgh != builtLayoutGridH_ || jgw < 1 ||
       jgh < 1)
   {
      popoutPanesRestoredThisSession_ = true;
      return;
   }
   const QJsonValue parr = o.value("panes");
   if (!parr.isArray())
   {
      popoutPanesRestoredThisSession_ = true;
      return;
   }
   for (const auto& v : parr.toArray())
   {
      if (!v.isObject())
      {
         continue;
      }
      const QJsonObject po   = v.toObject();
      const int         jidx = po.value("i").toInt(-1);
      if (jidx < 0)
      {
         continue;
      }
      const auto    idx = static_cast<std::size_t>(jidx);
      const QString gb  = po.value("g").toString();
      if (gb.isEmpty() || idx >= maps_.size() ||
          mapPanePoppedOut_.size() <= idx)
      {
         continue;
      }
      if (mapPanePoppedOut_.at(idx))
      {
         continue;
      }
      PopOutMap(idx);
      if (idx < mapPanePopoutFrames_.size() && mapPanePopoutFrames_.at(idx))
      {
         const QByteArray geom = QByteArray::fromBase64(gb.toUtf8());
         if (!geom.isEmpty())
         {
            mapPanePopoutFrames_.at(idx)->restoreGeometry(geom);
         }
      }
   }
   popoutPanesRestoredThisSession_ = true;
}

} // namespace scwx::qt::main

#include "main_window.moc"
