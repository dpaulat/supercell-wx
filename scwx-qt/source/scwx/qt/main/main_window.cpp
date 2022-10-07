#define NOMINMAX

#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/model/radar_product_model.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/ui/level2_products_widget.hpp>
#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/products.hpp>
#include <scwx/common/vcp.hpp>
#include <scwx/util/logger.hpp>

#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolButton>

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
       level2ProductsWidget_ {nullptr},
       level2SettingsWidget_ {nullptr},
       radarSiteDialog_ {nullptr},
       maps_ {},
       elevationCuts_ {},
       elevationButtonsChanged_ {false},
       resizeElevationButtons_ {false}
   {
      std::string appDataPath {
         QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            .toStdString()};
      std::string cacheDbPath {appDataPath + "/mbgl-cache.db"};

      if (!std::filesystem::exists(appDataPath))
      {
         if (!std::filesystem::create_directories(appDataPath))
         {
            logger_->error(
               "Unable to create application local data directory: \"{}\"",
               appDataPath);
         }
      }

      std::string mapboxApiKey =
         manager::SettingsManager::general_settings()->mapbox_api_key();

      settings_.resetToTemplate(QMapboxGLSettings::MapboxSettings);
      settings_.setApiKey(QString {mapboxApiKey.c_str()});
      settings_.setCacheDatabasePath(QString {cacheDbPath.c_str()});
      settings_.setCacheDatabaseMaximumSize(20 * 1024 * 1024);
   }
   ~MainWindowImpl() = default;

   void ConfigureMapLayout();
   void ConnectMapSignals();
   void HandleFocusChange(QWidget* focused);
   void SelectElevation(map::MapWidget* mapWidget, float elevation);
   void SelectRadarProduct(map::MapWidget*           mapWidget,
                           common::RadarProductGroup group,
                           const std::string&        productName,
                           int16_t                   productCode);
   void SetActiveMap(map::MapWidget* mapWidget);
   void UpdateAvailableLevel3Products();
   void UpdateElevationSelection(float elevation);
   void UpdateRadarProductSelection(common::RadarProductGroup group,
                                    const std::string&        product);
   void UpdateRadarProductSettings();
   void UpdateRadarSite();
   void UpdateVcp();

   MainWindow*       mainWindow_;
   QMapboxGLSettings settings_;
   map::MapWidget*   activeMap_;

   ui::Level2ProductsWidget* level2ProductsWidget_;
   ui::Level2SettingsWidget* level2SettingsWidget_;

   ui::Level3ProductsWidget* level3ProductsWidget_;

   ui::RadarSiteDialog* radarSiteDialog_;

   std::vector<map::MapWidget*> maps_;
   std::vector<float>           elevationCuts_;

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

   ui->vcpLabel->setVisible(false);
   ui->vcpValueLabel->setVisible(false);
   ui->vcpDescriptionLabel->setVisible(false);

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

   // Configure Docks
   ui->resourceExplorerDock->setVisible(false);

   ui->resourceTreeView->setModel(new model::RadarProductModel(this));

   // Configure Map
   p->ConfigureMapLayout();

   // Radar Site Dialog
   p->radarSiteDialog_ = new ui::RadarSiteDialog(this);

   // Add Level 2 Products
   p->level2ProductsWidget_ = new ui::Level2ProductsWidget(this);
   ui->radarProductGroupBox->layout()->replaceWidget(ui->level2ProductFrame,
                                                     p->level2ProductsWidget_);
   delete ui->level2ProductFrame;
   ui->level2ProductFrame = p->level2ProductsWidget_;

   // Add Level 3 Products
   p->level3ProductsWidget_ = new ui::Level3ProductsWidget(this);
   ui->radarProductGroupBox->layout()->replaceWidget(ui->level3ProductFrame,
                                                     p->level3ProductsWidget_);
   delete ui->level3ProductFrame;
   ui->level3ProductFrame = p->level3ProductsWidget_;

   // Add Level 2 Settings
   p->level2SettingsWidget_ = new ui::Level2SettingsWidget(ui->settingsFrame);
   ui->settingsFrame->layout()->addWidget(p->level2SettingsWidget_);
   p->level2SettingsWidget_->setVisible(false);

   auto mapSettings = manager::SettingsManager::map_settings();
   for (size_t i = 0; i < p->maps_.size(); i++)
   {
      p->SelectRadarProduct(p->maps_.at(i),
                            mapSettings->radar_product_group(i),
                            mapSettings->radar_product(i),
                            0);
   }

   p->ConnectMapSignals();

   connect(qApp,
           &QApplication::focusChanged,
           this,
           [=](QWidget* /*old*/, QWidget* now) { p->HandleFocusChange(now); });
   connect(p->level2ProductsWidget_,
           &ui::Level2ProductsWidget::RadarProductSelected,
           this,
           [&](common::RadarProductGroup group,
               const std::string&        productName,
               int16_t                   productCode) {
              p->SelectRadarProduct(
                 p->activeMap_, group, productName, productCode);
           });
   connect(p->level3ProductsWidget_,
           &ui::Level3ProductsWidget::RadarProductSelected,
           this,
           [&](common::RadarProductGroup group,
               const std::string&        productName,
               int16_t                   productCode) {
              p->SelectRadarProduct(
                 p->activeMap_, group, productName, productCode);
           });
   connect(p->level2SettingsWidget_,
           &ui::Level2SettingsWidget::ElevationSelected,
           this,
           [&](float elevation)
           { p->SelectElevation(p->activeMap_, elevation); });

   p->HandleFocusChange(p->activeMap_);
}

MainWindow::~MainWindow()
{
   delete ui;
}

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   resizeDocks({ui->radarToolboxDock}, {150}, Qt::Horizontal);
}

void MainWindow::on_actionOpen_triggered()
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
      [=]() { update(); },
      Qt::QueuedConnection);

   connect(
      dialog,
      &QFileDialog::fileSelected,
      this,
      [=](const QString& file)
      {
         logger_->info("Selected: {}", file.toStdString());

         std::shared_ptr<request::NexradFileRequest> request =
            std::make_shared<request::NexradFileRequest>();

         connect( //
            request.get(),
            &request::NexradFileRequest::RequestComplete,
            this,
            [=](std::shared_ptr<request::NexradFileRequest> request)
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

void MainWindow::on_actionExit_triggered()
{
   close();
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

void MainWindowImpl::ConfigureMapLayout()
{
   auto generalSettings = manager::SettingsManager::general_settings();

   const int64_t gridWidth  = generalSettings->grid_width();
   const int64_t gridHeight = generalSettings->grid_height();
   const int64_t mapCount   = gridWidth * gridHeight;

   size_t mapIndex = 0;

   QSplitter* vs = new QSplitter(Qt::Vertical);
   vs->setHandleWidth(1);

   maps_.resize(mapCount);

   auto MoveSplitter = [=](int /*pos*/, int /*index*/)
   {
      QSplitter* s = dynamic_cast<QSplitter*>(sender());

      if (s != nullptr)
      {
         auto sizes = s->sizes();
         for (QSplitter* hs : vs->findChildren<QSplitter*>())
         {
            hs->setSizes(sizes);
         }
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
            maps_[mapIndex] = new map::MapWidget(settings_);
         }

         hs->addWidget(maps_[mapIndex]);
      }

      connect(hs, &QSplitter::splitterMoved, this, MoveSplitter);
   }

   mainWindow_->ui->centralwidget->layout()->addWidget(vs);

   SetActiveMap(maps_.at(0));
}

void MainWindowImpl::ConnectMapSignals()
{
   std::for_each(maps_.cbegin(),
                 maps_.cend(),
                 [&](auto& mapWidget)
                 {
                    connect(mapWidget,
                            &map::MapWidget::MapParametersChanged,
                            this,
                            &MainWindowImpl::UpdateMapParameters);

                    connect(
                       mapWidget,
                       &map::MapWidget::RadarSweepUpdated,
                       this,
                       [&]()
                       {
                          if (mapWidget == activeMap_)
                          {
                             UpdateRadarProductSelection(
                                mapWidget->GetRadarProductGroup(),
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
                 });
}

void MainWindowImpl::HandleFocusChange(QWidget* focused)
{
   map::MapWidget* mapWidget = dynamic_cast<map::MapWidget*>(focused);

   if (mapWidget != nullptr)
   {
      SetActiveMap(mapWidget);
      UpdateAvailableLevel3Products();
      UpdateRadarProductSelection(mapWidget->GetRadarProductGroup(),
                                  mapWidget->GetRadarProductName());
      UpdateRadarProductSettings();
      UpdateRadarSite();
      UpdateVcp();
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

   mapWidget->SelectRadarProduct(group, productName, productCode);
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
      level2SettingsWidget_->setVisible(true);
   }
   else
   {
      level2SettingsWidget_->setVisible(false);
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
   }
   else
   {
      mainWindow_->ui->radarSiteValueLabel->setVisible(false);
      mainWindow_->ui->radarLocationLabel->setVisible(false);
   }
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
