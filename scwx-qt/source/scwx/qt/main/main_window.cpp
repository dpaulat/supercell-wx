#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/main/application.hpp>
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/alert_manager.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/update_manager.hpp>
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
#include <scwx/common/vcp.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <vector>
#include <set>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QAction>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

namespace scwx::qt::main
{

static const std::string logPrefix_ = "scwx::qt::main::main_window";
static const auto        logger_    = util::Logger::Create(logPrefix_);

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
   void ConfigureMapLayout();
   void ConfigureMapStyles();
   void ConfigureUiSettings();
   void ConnectAnimationSignals();
   void ConnectMapSignals();
   void ConnectOtherSignals();
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
   void SetActiveMap(map::MapWidget* mapWidget);
   void UpdateAvailableLevel3Products();
   void UpdateElevationSelection(float elevation);
   void UpdateMapStyle(const std::string& styleName);
   void UpdateRadarProductSelection(common::RadarProductGroup group,
                                    const std::string&        product);
   void UpdateRadarProductSettings();
   void UpdateRadarSite();
   void UpdateVcp();

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

   ui::AlertDockWidget*              alertDockWidget_ {};
   ui::MapAnnotationDockWidget*      mapAnnotationDock_ {};
   QAction*                          mapAnnotationOverlayAction_ {};
   ui::AnimationDockWidget*          animationDockWidget_ {};
   ui::AboutDialog*                  aboutDialog_ {};
   ui::ExportSettingsDialog*         exportSettingsDialog_ {};
   ui::GpsInfoDialog*                gpsInfoDialog_ {};
   ui::ImGuiDebugDialog*             imGuiDebugDialog_ {};
   ui::import::ImportSettingsWizard* importSettingsWizard_ {};
   ui::LayerDialog*                  layerDialog_ {};
   ui::PlacefileDialog*              placefileDialog_ {};
   ui::MarkerDialog*                 markerDialog_ {};
   ui::RadarSiteDialog*              radarSiteDialog_ {};
   ui::SettingsDialog*               settingsDialog_ {};
   ui::UpdateDialog*                 updateDialog_ {};

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

   std::chrono::system_clock::time_point selectedTime_ {};

public slots:
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

   const auto defaultTimeZone = p->activeMap_->GetDefaultTimeZone();

   // Configure Alert Dock
   p->alertDockWidget_ = new ui::AlertDockWidget(this);
   addDockWidget(Qt::BottomDockWidgetArea, p->alertDockWidget_);

   p->mapAnnotationDock_ = new ui::MapAnnotationDockWidget(p->activeMap_);
   p->mapAnnotationDock_->AttachToMap(p->activeMap_);
   p->mapAnnotationOverlayAction_ = new QAction(tr("&Draw Overlay"), this);
   p->mapAnnotationOverlayAction_->setCheckable(true);
   p->mapAnnotationOverlayAction_->setChecked(
      p->mapAnnotationDock_->OverlayVisible());
   QObject::connect(p->mapAnnotationOverlayAction_,
                    &QAction::toggled,
                    p->mapAnnotationDock_,
                    &ui::MapAnnotationDockWidget::SetOverlayVisible);
   MainWindowImpl* const impl = p.get();
   p->mapAnnotationDock_->SetBroadcastTargets(
      [impl]()
      {
         std::vector<std::shared_ptr<map::MapAnnotationLayer>> layers;
         for (map::MapWidget* mw : impl->maps_)
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
   for (map::MapWidget* mw : p->maps_)
   {
      if (mw == nullptr)
      {
         continue;
      }
      QObject::connect(mw,
                       &map::MapWidget::MapAnnotationLayerReady,
                       impl,
                       [impl, mw]()
                       {
                          if (impl->mapAnnotationDock_ == nullptr)
                          {
                             return;
                          }
                          if (mw == impl->activeMap_)
                          {
                             impl->mapAnnotationDock_->BindToLayer(
                                impl->activeMap_->map_annotation_layer(), false);
                          }
                          else
                          {
                             impl->mapAnnotationDock_->ReapplyToolAndStyleFromUi();
                          }
                       });
   }
   if (p->activeMap_ != nullptr)
   {
      p->mapAnnotationDock_->BindToLayer(p->activeMap_->map_annotation_layer(), false);
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
   ui->menuView->insertAction(ui->actionAlerts, p->mapAnnotationOverlayAction_);

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

   auto& mapSettings = settings::MapSettings::Instance();
   for (size_t i = 0; i < p->maps_.size(); i++)
   {
      p->SelectRadarProduct(p->maps_.at(i),
                            common::GetRadarProductGroup(
                               mapSettings.radar_product_group(i).GetValue()),
                            mapSettings.radar_product(i).GetValue(),
                            0);
   }

   p->PopulateMapStyles();
   p->ConfigureMapStyles();
   p->ConfigureUiSettings();
   p->ConnectMapSignals();
   p->ConnectAnimationSignals();
   p->ConnectOtherSignals();
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

void MainWindowImpl::ConfigureMapLayout()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   const int64_t gridWidth  = generalSettings.grid_width().GetValue();
   const int64_t gridHeight = generalSettings.grid_height().GetValue();
   const int64_t mapCount   = gridWidth * gridHeight;

   size_t mapIndex = 0;

   QSplitter* vs = new QSplitter(Qt::Vertical);
   vs->setHandleWidth(1);

   maps_.resize(mapCount);
   timelineManager_->SetMapCount(mapCount);

   auto MoveSplitter = [=, this](int /*pos*/, int /*index*/)
   {
      QSplitter* s = static_cast<QSplitter*>(sender());

      auto sizes = s->sizes();
      for (QSplitter* hs : vs->findChildren<QSplitter*>())
      {
         hs->setSizes(sizes);
      }
   };

   glContext_ = std::make_shared<gl::GlContext>();

   for (int64_t y = 0; y < gridHeight; y++)
   {
      QSplitter* hs = new QSplitter(vs);
      hs->setHandleWidth(1);

      for (int64_t x = 0; x < gridWidth; x++, mapIndex++)
      {
         if (maps_.at(mapIndex) == nullptr)
         {
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Owned by parent
            maps_[mapIndex] =
               new map::MapWidget(mapIndex, settings_, glContext_);
         }

         hs->addWidget(maps_[mapIndex]);
      }

      connect(hs, &QSplitter::splitterMoved, this, MoveSplitter);
   }

   mainWindow_->ui->centralwidget->layout()->addWidget(vs);

   if (mapCount > 0)
   {
      SetActiveMap(maps_.at(0));
   }
   else
   {
      SetActiveMap(nullptr);
   }
}

void MainWindowImpl::ConfigureMapStyles()
{
   const auto& mapProviderInfo = map::GetMapProviderInfo(mapProvider_);
   auto&       mapSettings     = settings::MapSettings::Instance();

   for (std::size_t i = 0; i < maps_.size(); i++)
   {
      const std::string configuredStyleName =
         mapSettings.map_style(i).GetValue();
      std::string styleName = configuredStyleName;

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

      const std::string currentStyleName = maps_.at(i)->GetMapStyle();
      if (currentStyleName != "?")
      {
         styleName = currentStyleName;
      }

      maps_.at(i)->SetInitialMapStyle(styleName);

      if (maps_[i] == activeMap_)
      {
         UpdateMapStyle(styleName);
      }

      if (configuredStyleName != styleName)
      {
         mapSettings.map_style(i).StageValue(styleName);
      }
   }
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
      connect(
         mapWidget,
         &map::MapWidget::MapParametersChanged,
         this,
         [&](double latitude, double longitude)
         {
            if (mapWidget == activeMap_)
            {
               Q_EMIT mainWindow_->ActiveMapMoved(latitude, longitude);
            }
         },
         Qt::QueuedConnection);

      connect(mapWidget,
              &map::MapWidget::MapStyleChanged,
              this,
              [&](const std::string& mapStyle)
              {
                 if (mapWidget == activeMap_)
                 {
                    UpdateMapStyle(mapStyle);
                 }
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
                 for (map::MapWidget* map : maps_)
                 {
                    map->SelectRadarSite(id, updateCoordinates);
                 }

                 UpdateRadarSite();
              });
   }
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
   connect(mainWindow_->ui->mapStyleComboBox,
           &QComboBox::currentTextChanged,
           mainWindow_,
           [&](const QString& text)
           {
              activeMap_->SetMapStyle(text.toStdString());

              // Update settings for active map
              for (std::size_t i = 0; i < maps_.size(); ++i)
              {
                 if (maps_[i] == activeMap_)
                 {
                    auto& mapSettings = settings::MapSettings::Instance();
                    mapSettings.map_style(i).StageValue(text.toStdString());
                    break;
                 }
              }
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
   connect(radarSiteDialog_,
           &ui::RadarSiteDialog::accepted,
           this,
           [&]()
           {
              std::string selectedRadarSite = radarSiteDialog_->radar_site();

              for (map::MapWidget* map : maps_)
              {
                 map->SelectRadarSite(selectedRadarSite);
              }

              UpdateRadarSite();
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
         }));

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
              for (map::MapWidget* map : maps_)
              {
                 map->SelectRadarSite(siteId);
              }

              UpdateRadarSite();
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
   if (mapWidget == activeMap_)
   {
      return;
   }

   activeMap_ = mapWidget;

   for (map::MapWidget* widget : maps_)
   {
      widget->SetActive(mapWidget == widget);
   }

   if (mapAnnotationDock_ != nullptr)
   {
      if (activeMap_ != nullptr)
      {
         mapAnnotationDock_->AttachToMap(activeMap_);
         mapAnnotationDock_->BindToLayer(activeMap_->map_annotation_layer(), false);
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

void MainWindowImpl::UpdateMapParameters(
   double latitude, double longitude, double zoom, double bearing, double pitch)
{
   for (map::MapWidget* map : maps_)
   {
      map->SetMapParameters(latitude, longitude, zoom, bearing, pitch);
   }
}

void MainWindowImpl::UpdateMapStyle(const std::string& styleName)
{
   int index = mainWindow_->ui->mapStyleComboBox->findText(
      QString::fromStdString(styleName));
   if (index != -1)
   {
      const QSignalBlocker blocker(mainWindow_->ui->mapStyleComboBox);
      mainWindow_->ui->mapStyleComboBox->setCurrentIndex(index);

      // Update settings for active map
      for (std::size_t i = 0; i < maps_.size(); ++i)
      {
         if (maps_[i] == activeMap_)
         {
            auto& mapSettings = settings::MapSettings::Instance();
            mapSettings.map_style(i).StageValue(styleName);
            break;
         }
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
   const std::string                  homeRadarSite =
      settings::GeneralSettings::Instance().default_radar_site().GetValue();

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

      timelineManager_->SetRadarSite("?");
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

} // namespace scwx::qt::main

#include "main_window.moc"
