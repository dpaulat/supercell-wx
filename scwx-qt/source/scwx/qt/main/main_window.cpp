#define NOMINMAX

#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/main/application.hpp>
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/update_manager.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/model/radar_product_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/ui_settings.hpp>
#include <scwx/qt/ui/about_dialog.hpp>
#include <scwx/qt/ui/alert_dock_widget.hpp>
#include <scwx/qt/ui/animation_dock_widget.hpp>
#include <scwx/qt/ui/collapsible_group.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
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

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
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
       imGuiDebugDialog_ {nullptr},
       layerDialog_ {nullptr},
       placefileDialog_ {nullptr},
       radarSiteDialog_ {nullptr},
       settingsDialog_ {nullptr},
       updateDialog_ {nullptr},
       radarProductModel_ {nullptr},
       placefileManager_ {manager::PlacefileManager::Instance()},
       textEventManager_ {manager::TextEventManager::Instance()},
       timelineManager_ {manager::TimelineManager::Instance()},
       updateManager_ {manager::UpdateManager::Instance()},
       maps_ {},
       elevationCuts_ {},
       elevationButtonsChanged_ {false},
       resizeElevationButtons_ {false}
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

      settings_.resetToTemplate(mapProviderInfo.settingsTemplate_);
      settings_.setApiKey(QString {mapProviderApiKey.c_str()});
      settings_.setCacheDatabasePath(QString {cacheDbPath.c_str()});
      settings_.setCacheDatabaseMaximumSize(20 * 1024 * 1024);
   }
   ~MainWindowImpl() { threadPool_.join(); }

   void AsyncSetup();
   void ConfigureMapLayout();
   void ConfigureMapStyles();
   void ConfigureUiSettings();
   void ConnectAnimationSignals();
   void ConnectMapSignals();
   void ConnectOtherSignals();
   void HandleFocusChange(QWidget* focused);
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

   MainWindow*           mainWindow_;
   QMapLibreGL::Settings settings_;
   map::MapProvider      mapProvider_;
   map::MapWidget*       activeMap_;

   ui::CollapsibleGroup*     mapSettingsGroup_;
   ui::CollapsibleGroup*     level2ProductsGroup_;
   ui::CollapsibleGroup*     level2SettingsGroup_;
   ui::CollapsibleGroup*     level3ProductsGroup_;
   ui::CollapsibleGroup*     timelineGroup_;
   ui::Level2ProductsWidget* level2ProductsWidget_;
   ui::Level2SettingsWidget* level2SettingsWidget_;

   ui::Level3ProductsWidget* level3ProductsWidget_;

   ui::AlertDockWidget*     alertDockWidget_;
   ui::AnimationDockWidget* animationDockWidget_;
   ui::AboutDialog*         aboutDialog_;
   ui::ImGuiDebugDialog*    imGuiDebugDialog_;
   ui::LayerDialog*         layerDialog_;
   ui::PlacefileDialog*     placefileDialog_;
   ui::RadarSiteDialog*     radarSiteDialog_;
   ui::SettingsDialog*      settingsDialog_;
   ui::UpdateDialog*        updateDialog_;

   std::unique_ptr<model::RadarProductModel>  radarProductModel_;
   std::shared_ptr<manager::PlacefileManager> placefileManager_;
   std::shared_ptr<manager::TextEventManager> textEventManager_;
   std::shared_ptr<manager::TimelineManager>  timelineManager_;
   std::shared_ptr<manager::UpdateManager>    updateManager_;

   std::vector<map::MapWidget*> maps_;
   std::vector<float>           elevationCuts_;

   std::chrono::system_clock::time_point volumeTime_ {};

   bool elevationButtonsChanged_;
   bool resizeElevationButtons_;

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

   // Assign the bottom left corner to the left dock widget
   setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

   ui->vcpLabel->setVisible(false);
   ui->vcpValueLabel->setVisible(false);
   ui->vcpDescriptionLabel->setVisible(false);

   // Configure Alert Dock
   p->alertDockWidget_ = new ui::AlertDockWidget(this);
   p->alertDockWidget_->setVisible(false);
   addDockWidget(Qt::BottomDockWidgetArea, p->alertDockWidget_);

   // Configure Menu
   ui->menuView->insertAction(ui->actionRadarToolbox,
                              ui->radarToolboxDock->toggleViewAction());
   ui->radarToolboxDock->toggleViewAction()->setText(tr("Radar &Toolbox"));
   ui->actionRadarToolbox->setVisible(false);

   ui->menuView->insertAction(ui->actionResourceExplorer,
                              ui->resourceExplorerDock->toggleViewAction());
   ui->resourceExplorerDock->toggleViewAction()->setText(
      tr("&Resource Explorer"));
   ui->actionResourceExplorer->setVisible(false);
   ui->resourceExplorerDock->toggleViewAction()->setVisible(false);

   ui->menuView->insertAction(ui->actionAlerts,
                              p->alertDockWidget_->toggleViewAction());
   p->alertDockWidget_->toggleViewAction()->setText(tr("&Alerts"));
   ui->actionAlerts->setVisible(false);

   ui->menuDebug->menuAction()->setVisible(
      settings::GeneralSettings::Instance().debug_enabled().GetValue());

   // Configure Resource Explorer Dock
   ui->resourceExplorerDock->setVisible(false);

   p->radarProductModel_ = std::make_unique<model::RadarProductModel>();
   ui->resourceTreeView->setModel(p->radarProductModel_->model());

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
   ui->radarToolboxScrollAreaContents->layout()->replaceWidget(
      ui->mapSettingsGroupBox, p->mapSettingsGroup_);
   ui->mapSettingsGroupBox->setVisible(false);

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

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   resizeDocks({ui->radarToolboxDock}, {188}, Qt::Horizontal);
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

         std::shared_ptr<request::NexradFileRequest> request =
            std::make_shared<request::NexradFileRequest>();

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
      });
}

void MainWindow::on_actionAboutSupercellWx_triggered()
{
   p->aboutDialog_->show();
}

void MainWindow::on_radarSiteSelectButton_clicked()
{
   p->radarSiteDialog_->show();
}

void MainWindow::on_resourceTreeCollapseAllButton_clicked()
{
   ui->resourceTreeView->collapseAll();
}

void MainWindow::on_resourceTreeExpandAllButton_clicked()
{
   ui->resourceTreeView->expandAll();
}

void MainWindow::on_resourceTreeView_doubleClicked(const QModelIndex& index)
{
   std::string selectedString {index.data().toString().toStdString()};
   std::chrono::system_clock::time_point time {};

   logger_->debug("Selecting resource: {}",
                  index.data().toString().toStdString());

   static const std::string timeFormat {"%Y-%m-%d %H:%M:%S"};

   using namespace std::chrono;

#if !defined(_MSC_VER)
   using namespace date;
#endif

   std::istringstream in {selectedString};
   in >> parse(timeFormat, time);

   if (in.fail())
   {
      // Not a time string, ignore double-click
      return;
   }

   QModelIndex parent1 = index.parent();
   QModelIndex parent2 = parent1.parent();
   QModelIndex parent3 = parent2.parent();

   std::string radarSite {};
   std::string groupName {};
   std::string product {};

   if (!parent2.isValid())
   {
      // A time entry should be at the third or fourth level
      logger_->error("Unexpected resource data");
      return;
   }

   if (parent3.isValid())
   {
      // Level 3 Product
      radarSite = parent3.data().toString().toStdString();
      groupName = parent2.data().toString().toStdString();
      product   = parent1.data().toString().toStdString();
   }
   else
   {
      // Level 2 Product
      radarSite = parent2.data().toString().toStdString();
      groupName = parent1.data().toString().toStdString();
      // No product index
   }

   common::RadarProductGroup group = common::GetRadarProductGroup(groupName);

   // Update radar site if different from currently selected
   if (p->activeMap_->GetRadarSite()->id() != radarSite)
   {
      p->activeMap_->SelectRadarSite(radarSite);
   }

   // Select the updated radar product
   p->activeMap_->SelectRadarProduct(group, product, 0, time);
}

void MainWindowImpl::AsyncSetup()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   // Check for updates
   if (generalSettings.update_notifications_enabled().GetValue())
   {
      boost::asio::post(
         threadPool_,
         [this]() { updateManager_->CheckForUpdates(main::kVersionString_); });
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

   SetActiveMap(maps_.at(0));
}

void MainWindowImpl::ConfigureMapStyles()
{
   const auto& mapProviderInfo = map::GetMapProviderInfo(mapProvider_);
   auto&       mapSettings     = settings::MapSettings::Instance();

   for (std::size_t i = 0; i < maps_.size(); i++)
   {
      std::string styleName = mapSettings.map_style(i).GetValue();

      if (std::find_if(mapProviderInfo.mapStyles_.cbegin(),
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
              for (auto map : maps_)
              {
                 volumeTime_ = dateTime;
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
   connect(updateManager_.get(),
           &manager::UpdateManager::UpdateAvailable,
           this,
           [this](const std::string&        latestVersion,
                  const types::gh::Release& latestRelease)
           {
              updateDialog_->UpdateReleaseInfo(latestVersion, latestRelease);
              updateDialog_->show();
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

void MainWindowImpl::PopulateMapStyles()
{
   const auto& mapProviderInfo = map::GetMapProviderInfo(mapProvider_);
   for (const auto& mapStyle : mapProviderInfo.mapStyles_)
   {
      mainWindow_->ui->mapStyleComboBox->addItem(
         QString::fromStdString(mapStyle.name_));
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
      mainWindow_->ui->radarSiteValueLabel->setVisible(true);
      mainWindow_->ui->radarLocationLabel->setVisible(true);

      mainWindow_->ui->radarSiteValueLabel->setText(radarSite->id().c_str());
      mainWindow_->ui->radarLocationLabel->setText(
         radarSite->location_name().c_str());

      timelineManager_->SetRadarSite(radarSite->id());
   }
   else
   {
      mainWindow_->ui->radarSiteValueLabel->setVisible(false);
      mainWindow_->ui->radarLocationLabel->setVisible(false);

      timelineManager_->SetRadarSite("?");
   }

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
