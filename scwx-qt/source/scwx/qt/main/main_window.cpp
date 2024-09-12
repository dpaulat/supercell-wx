#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/main/application.hpp>
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/alert_manager.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
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
#include <scwx/qt/settings/ui_settings.hpp>
#include <scwx/qt/ui/about_dialog.hpp>
#include <scwx/qt/ui/alert_dock_widget.hpp>
#include <scwx/qt/ui/animation_dock_widget.hpp>
#include <scwx/qt/ui/collapsible_group.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/ui/gps_info_dialog.hpp>
#include <scwx/qt/ui/imgui_debug_dialog.hpp>
#include <scwx/qt/ui/layer_dialog.hpp>
#include <scwx/qt/ui/level2_products_widget.hpp>
#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/placefile_dialog.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
#include <scwx/qt/ui/settings_dialog.hpp>
#include <scwx/qt/ui/update_dialog.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/products.hpp>
#include <scwx/common/vcp.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <set>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>

#if !defined(_MSC_VER)
#   include <date/date.h>
#endif

namespace scwx
{
namespace qt
{
namespace main
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
       mapSettingsGroup_ {nullptr},
       level2ProductsGroup_ {nullptr},
       level2SettingsGroup_ {nullptr},
       level3ProductsGroup_ {nullptr},
       timelineGroup_ {nullptr},
       level2ProductsWidget_ {nullptr},
       level2SettingsWidget_ {nullptr},
       level3ProductsWidget_ {nullptr},
       alertDockWidget_ {nullptr},
       animationDockWidget_ {nullptr},
       aboutDialog_ {nullptr},
       gpsInfoDialog_ {nullptr},
       imGuiDebugDialog_ {nullptr},
       layerDialog_ {nullptr},
       placefileDialog_ {nullptr},
       radarSiteDialog_ {nullptr},
       settingsDialog_ {nullptr},
       updateDialog_ {nullptr},
       alertManager_ {manager::AlertManager::Instance()},
       placefileManager_ {manager::PlacefileManager::Instance()},
       positionManager_ {manager::PositionManager::Instance()},
       textEventManager_ {manager::TextEventManager::Instance()},
       timelineManager_ {manager::TimelineManager::Instance()},
       updateManager_ {manager::UpdateManager::Instance()},
       maps_ {}
   {
      mapProvider_ = map::GetMapProvider(
         settings::GeneralSettings::Instance().map_provider().GetValue());
      const map::MapProviderInfo& mapProviderInfo =
         map::GetMapProviderInfo(mapProvider_);

      std::string appDataPath {
         QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            .toStdString()};
      std::string cacheDbPath {appDataPath + "/" +
                               mapProviderInfo.cacheDbName_};

      if (!std::filesystem::exists(appDataPath))
      {
         if (!std::filesystem::create_directories(appDataPath))
         {
            logger_->error(
               "Unable to create application local data directory: \"{}\"",
               appDataPath);
         }
      }

      std::string mapProviderApiKey = map::GetMapProviderApiKey(mapProvider_);

      if (mapProvider_ == map::MapProvider::Mapbox)
      {
         settings_.setProviderTemplate(mapProviderInfo.providerTemplate_);
         settings_.setApiKey(QString {mapProviderApiKey.c_str()});
      }
      settings_.setCacheDatabasePath(QString {cacheDbPath.c_str()});
      settings_.setCacheDatabaseMaximumSize(20 * 1024 * 1024);

      if (settings::GeneralSettings::Instance().track_location().GetValue())
      {
         positionManager_->TrackLocation(true);
      }
   }
   ~MainWindowImpl()
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      auto& customStyleUrl       = generalSettings.custom_style_url();
      auto& customStyleDrawLayer = generalSettings.custom_style_draw_layer();

      customStyleUrl.UnregisterValueChangedCallback(
         customStyleUrlChangedCallbackUuid_);
      customStyleDrawLayer.UnregisterValueChangedCallback(
         customStyleDrawLayerChangedCallbackUuid_);

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
   void SelectElevation(map::MapWidget* mapWidget, float elevation);
   void SelectRadarProduct(map::MapWidget*           mapWidget,
                           common::RadarProductGroup group,
                           const std::string&        productName,
                           int16_t                   productCode);
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

   ui::CollapsibleGroup*     mapSettingsGroup_;
   ui::CollapsibleGroup*     level2ProductsGroup_;
   ui::CollapsibleGroup*     level2SettingsGroup_;
   ui::CollapsibleGroup*     level3ProductsGroup_;
   ui::CollapsibleGroup*     timelineGroup_;
   ui::Level2ProductsWidget* level2ProductsWidget_;
   ui::Level2SettingsWidget* level2SettingsWidget_;

   ui::Level3ProductsWidget* level3ProductsWidget_;

   QLabel* coordinateLabel_ {nullptr};
   QLabel* timeLabel_ {nullptr};

   ui::AlertDockWidget*     alertDockWidget_;
   ui::AnimationDockWidget* animationDockWidget_;
   ui::AboutDialog*         aboutDialog_;
   ui::GpsInfoDialog*       gpsInfoDialog_;
   ui::ImGuiDebugDialog*    imGuiDebugDialog_;
   ui::LayerDialog*         layerDialog_;
   ui::PlacefileDialog*     placefileDialog_;
   ui::RadarSiteDialog*     radarSiteDialog_;
   ui::SettingsDialog*      settingsDialog_;
   ui::UpdateDialog*        updateDialog_;

   QTimer clockTimer_ {};

   bool               customStyleAvailable_ {false};
   boost::uuids::uuid customStyleDrawLayerChangedCallbackUuid_ {};
   boost::uuids::uuid customStyleUrlChangedCallbackUuid_ {};

   std::shared_ptr<manager::AlertManager>  alertManager_;
   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};
   std::shared_ptr<manager::PlacefileManager> placefileManager_;
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

   std::vector<map::MapWidget*> maps_;

   std::chrono::system_clock::time_point volumeTime_ {};

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

   p->radarSitePresetsMenu_ = new QMenu(this);
   ui->radarSitePresetsButton->setMenu(p->radarSitePresetsMenu_);

   auto radarSitePresets = p->radarSiteModel_->presets();
   for (auto& preset : radarSitePresets)
   {
      p->AddRadarSitePreset(preset);
   }

   ui->radarSitePresetsButton->setVisible(!radarSitePresets.empty());

   auto& uiSettings = settings::UiSettings::Instance();
   // Configure Alert Dock
   bool alertDockVisible_ = uiSettings.alert_dock_visible().GetValue();
   p->alertDockWidget_ = new ui::AlertDockWidget(this);
   p->alertDockWidget_->setVisible(false);
   addDockWidget(Qt::BottomDockWidgetArea, p->alertDockWidget_);
   p->alertDockWidget_->setVisible(alertDockVisible_);

   // GPS Info Dialog
   p->gpsInfoDialog_ = new ui::GpsInfoDialog(this);

   // Configure Menu
   ui->menuView->insertAction(ui->actionRadarToolbox,
                              ui->radarToolboxDock->toggleViewAction());
   ui->radarToolboxDock->toggleViewAction()->setText(tr("Radar &Toolbox"));
   ui->actionRadarToolbox->setVisible(false);
   ui->radarToolboxDock->setVisible(
      uiSettings.radar_toolbox_dock_visible().GetValue());

   // Update dock setting on visiblity change.
   connect(ui->radarToolboxDock->toggleViewAction(),
           &QAction::triggered,
           this,
           [](bool checked)
           {
              settings::UiSettings::Instance()
                 .radar_toolbox_dock_visible()
                 .StageValue(checked);
           });

   ui->menuView->insertAction(ui->actionAlerts,
                              p->alertDockWidget_->toggleViewAction());
   p->alertDockWidget_->toggleViewAction()->setText(tr("&Alerts"));
   ui->actionAlerts->setVisible(false);

   ui->menuDebug->menuAction()->setVisible(
      settings::GeneralSettings::Instance().debug_enabled().GetValue());

   // Configure Map
   p->ConfigureMapLayout();

   // Radar Site Dialog
   p->radarSiteDialog_ = new ui::RadarSiteDialog(this);

   // Placefile Manager Dialog
   p->placefileDialog_ = new ui::PlacefileDialog(this);

   // Layer Dialog
   p->layerDialog_ = new ui::LayerDialog(this);

   // Settings Dialog
   p->settingsDialog_ = new ui::SettingsDialog(this);

   // Map Settings
   p->mapSettingsGroup_ = new ui::CollapsibleGroup(tr("Map Settings"), this);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(ui->mapStyleLabel);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(ui->mapStyleComboBox);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(
      ui->trackLocationCheckBox);
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

   // Timeline
   p->timelineGroup_       = new ui::CollapsibleGroup(tr("Timeline"), this);
   p->animationDockWidget_ = new ui::AnimationDockWidget(this);
   p->timelineGroup_->GetContentsLayout()->addWidget(p->animationDockWidget_);
   ui->radarToolboxScrollAreaContents->layout()->addWidget(p->timelineGroup_);

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

   // restore the UI state
   std::string uiState =
      settings::UiSettings::Instance().main_ui_state().GetValue();

   bool restored =
      restoreState(QByteArray::fromBase64(QByteArray::fromStdString(uiState)));
   if (!restored)
   {
      resizeDocks({ui->radarToolboxDock}, {194}, Qt::Horizontal);
   }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
   // save the UI state
   QByteArray uiState = saveState().toBase64();
   settings::UiSettings::Instance().main_ui_state().StageValue(uiState.data());

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
   connect(
      dialog,
      &QFileDialog::finished,
      this,
      [this]() { update(); },
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
   connect(
      dialog,
      &QFileDialog::finished,
      this,
      [this]() { update(); },
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

   for (int64_t y = 0; y < gridHeight; y++)
   {
      QSplitter* hs = new QSplitter(vs);
      hs->setHandleWidth(1);

      for (int64_t x = 0; x < gridWidth; x++, mapIndex++)
      {
         if (maps_.at(mapIndex) == nullptr)
         {
            maps_[mapIndex] = new map::MapWidget(mapIndex, settings_);
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
      std::string styleName = mapSettings.map_style(i).GetValue();

      if ((customStyleAvailable_ && styleName == "Custom") ||
          std::find_if(mapProviderInfo.mapStyles_.cbegin(),
                       mapProviderInfo.mapStyles_.cend(),
                       [&](const auto& mapStyle) {
                          return mapStyle.name_ == styleName;
                       }) != mapProviderInfo.mapStyles_.cend())
      {
         // Initialize map style from settings
         maps_.at(i)->SetInitialMapStyle(styleName);

         // Update the active map's style
         if (maps_[i] == activeMap_)
         {
            UpdateMapStyle(styleName);
         }
      }
      else if (!mapProviderInfo.mapStyles_.empty())
      {
         // Stage first valid map style from map provider
         mapSettings.map_style(i).StageValue(
            mapProviderInfo.mapStyles_.at(0).name_);
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
           [this]()
           {
              for (auto map : maps_)
              {
                 map->update();
              }
           });
   connect(timelineManager_.get(),
           &manager::TimelineManager::VolumeTimeUpdated,
           [this](std::chrono::system_clock::time_point dateTime)
           {
              volumeTime_ = dateTime;
              for (auto map : maps_)
              {
                 map->SelectTime(dateTime);
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
   connect(mainWindow_->ui->trackLocationCheckBox,
           &QCheckBox::stateChanged,
           mainWindow_,
           [this](int state)
           {
              bool trackingEnabled = (state == Qt::CheckState::Checked);

              settings::GeneralSettings::Instance().track_location().StageValue(
                 trackingEnabled);

              // Turn on location tracking
              positionManager_->TrackLocation(trackingEnabled);
           });
   connect(level2ProductsWidget_,
           &ui::Level2ProductsWidget::RadarProductSelected,
           mainWindow_,
           [&](common::RadarProductGroup group,
               const std::string&        productName,
               int16_t                   productCode) {
              SelectRadarProduct(activeMap_, group, productName, productCode);
           });
   connect(level3ProductsWidget_,
           &ui::Level3ProductsWidget::RadarProductSelected,
           mainWindow_,
           [&](common::RadarProductGroup group,
               const std::string&        productName,
               int16_t                   productCode) {
              SelectRadarProduct(activeMap_, group, productName, productCode);
           });
   connect(level2SettingsWidget_,
           &ui::Level2SettingsWidget::ElevationSelected,
           mainWindow_,
           [&](float elevation) { SelectElevation(activeMap_, elevation); });
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
              timeLabel_->setText(QString::fromStdString(
                 util::TimeString(std::chrono::system_clock::now())));
              timeLabel_->setVisible(true);
           });
   clockTimer_.start(1000);
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
   const auto& mapProviderInfo = map::GetMapProviderInfo(mapProvider_);
   for (const auto& mapStyle : mapProviderInfo.mapStyles_)
   {
      mainWindow_->ui->mapStyleComboBox->addItem(
         QString::fromStdString(mapStyle.name_));
   }

   auto& generalSettings = settings::GeneralSettings::Instance();

   auto& customStyleUrl       = generalSettings.custom_style_url();
   auto& customStyleDrawLayer = generalSettings.custom_style_draw_layer();

   customStyleUrlChangedCallbackUuid_ =
      customStyleUrl.RegisterValueChangedCallback(
         [this]([[maybe_unused]] const std::string& value)
         { PopulateCustomMapStyle(); });
   customStyleDrawLayerChangedCallbackUuid_ =
      customStyleDrawLayer.RegisterValueChangedCallback(
         [this]([[maybe_unused]] const std::string& value)
         { PopulateCustomMapStyle(); });

   PopulateCustomMapStyle();
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

   mapWidget->SelectRadarProduct(group, productName, productCode, volumeTime_);
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
   if (activeMap_->GetRadarProductGroup() == common::RadarProductGroup::Level2)
   {
      level2SettingsWidget_->UpdateSettings(activeMap_);
      level2SettingsGroup_->setVisible(true);
   }
   else
   {
      level2SettingsGroup_->setVisible(false);
   }
}

void MainWindowImpl::UpdateRadarSite()
{
   std::shared_ptr<config::RadarSite> radarSite = activeMap_->GetRadarSite();

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
   }
   else
   {
      mainWindow_->setWindowTitle(tr("Supercell Wx"));

      mainWindow_->ui->radarSiteValueLabel->setVisible(false);
      mainWindow_->ui->radarLocationLabel->setVisible(false);

      timelineManager_->SetRadarSite("?");
   }

   alertManager_->SetRadarSite(radarSite);
   placefileManager_->SetRadarSite(radarSite);
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

} // namespace main
} // namespace qt
} // namespace scwx

#include "main_window.moc"
