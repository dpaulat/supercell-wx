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
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/manager/spc_outlook_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/update_manager.hpp>
#include <scwx/qt/settings/spc_outlook_settings.hpp>
#include <scwx/spc/spc_types.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/model/radar_site_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/radar_preset_settings.hpp>
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
#include <scwx/qt/ui/level2_products_widget.hpp>
#include <scwx/qt/ui/level2_settings_widget.hpp>
#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/level3_settings_widget.hpp>
#include <scwx/qt/ui/placefile_dialog.hpp>
#include <scwx/qt/ui/marker_dialog.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
#include <scwx/qt/ui/settings_dialog.hpp>
#include <scwx/qt/ui/sounding_panel.hpp>
#include <scwx/qt/ui/update_dialog.hpp>
#include <scwx/qt/ui/import/import_settings_wizard.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/products.hpp>
#include <scwx/common/vcp.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <set>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
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
   Q_DISABLE_COPY_MOVE(MainWindowImpl)

   explicit MainWindowImpl(MainWindow* mainWindow) :
       mainWindow_ {mainWindow},
       settings_ {},
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
      gridWidthConnection_.disconnect();
      gridHeightConnection_.disconnect();
      for (auto& connection : connections_)
      {
         connection.disconnect();
      }

      clockTimer_.stop();
      threadPool_.join();
   }

   void AddRadarSitePreset(const std::string& id);
   void AsyncSetup();
   void InitializeGlContext();
   void CreateAllMapWidgets();
   void RebuildMapLayout(std::int64_t gridWidth,
                         std::int64_t gridHeight,
                         QList<int>   rowSizes    = {},
                         QList<int>   columnSizes = {});
   void ConfigureMapStyles();
   void ConfigureUiSettings();
   void ConnectAnimationSignals();
   void ConnectMapSignals();
   void ConnectMapSignalsForWidget(map::MapWidget* mapWidget);
   void ConnectAnimationSignalsForWidget(map::MapWidget* mapWidget,
                                         std::size_t     index);
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

   // Preset methods
   void PopulatePresetComboBox();
   void OnSavePreset();
   void OnLoadPreset();
   void OnRenamePreset();
   void OnDeletePreset();
   void ClearActivePreset();

   boost::asio::thread_pool threadPool_ {1u};

   MainWindow*         mainWindow_;
   QMapLibre::Settings settings_;
   map::MapProvider    mapProvider_;
   map::MapWidget*     activeMap_ {nullptr};

   std::shared_ptr<gl::GlContext> glContext_ {nullptr};

   ui::CollapsibleGroup*     mapSettingsGroup_ {nullptr};
   ui::CollapsibleGroup*     level2ProductsGroup_ {nullptr};
   ui::CollapsibleGroup*     level2SettingsGroup_ {nullptr};
   ui::CollapsibleGroup*     level3ProductsGroup_ {nullptr};
   ui::CollapsibleGroup*     level3SettingsGroup_ {nullptr};
   ui::CollapsibleGroup*     timelineGroup_ {nullptr};
   ui::CollapsibleGroup*     spcOutlookGroup_ {nullptr};
   QComboBox*                spcDayCombo_ {nullptr};
   QComboBox*                spcProductCombo_ {nullptr};
   QSlider*                  spcOpacitySlider_ {nullptr};
   QCheckBox*                spcAutoRefreshCheck_ {nullptr};
   ui::Level2ProductsWidget* level2ProductsWidget_ {nullptr};
   ui::Level2SettingsWidget* level2SettingsWidget_ {nullptr};

   ui::Level3ProductsWidget* level3ProductsWidget_ {nullptr};
   ui::Level3SettingsWidget* level3SettingsWidget_ {nullptr};

   QLabel* coordinateLabel_ {nullptr};
   QLabel* timeLabel_ {nullptr};

   ui::AlertDockWidget*              alertDockWidget_ {};
   ui::AnimationDockWidget*          animationDockWidget_ {};
   ui::SoundingPanel*                soundingPanel_ {};
   bool                              selectingSoundingPoint_ {false};
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

   bool   applyingGridChange_ {false};
   QTimer gridRebuildTimer_ {};

   boost::signals2::scoped_connection homeRadarConnection_ {};
   boost::signals2::scoped_connection clockFormatConnection_ {};
   boost::signals2::scoped_connection defaultTimeZoneConnection_ {};
   boost::signals2::scoped_connection gridWidthConnection_ {};
   boost::signals2::scoped_connection gridHeightConnection_ {};

   std::vector<map::MapWidget*> maps_;
   QWidget*                     mapContainer_ {nullptr};

   std::chrono::system_clock::time_point selectedTime_ {};

   bool   firstShow_ {true};
   bool   applyingDockWidth_ {false};
   QTimer dockWidthSaveTimer_ {};

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
   auto& generalSettings = settings::GeneralSettings::Instance();
   p->InitializeGlContext();

   // Hidden container for MapWidgets not currently in the layout
   p->mapContainer_ = new QWidget(this);
   p->mapContainer_->setVisible(false);

   // Pre-create all 9 MapWidgets once — avoids delete/recreate issues
   p->CreateAllMapWidgets();

   // Check if an active preset exists — use its grid for initial layout
   auto& presetSettings = settings::RadarPresetSettings::Instance();
   auto  presetIndex =
      presetSettings.FindPresetIndex(presetSettings.active_preset());

   if (presetIndex.has_value())
   {
      auto& preset = presetSettings.presets().at(*presetIndex);
      p->RebuildMapLayout(preset.gridWidth, preset.gridHeight);
   }
   else
   {
      p->RebuildMapLayout(generalSettings.grid_width().GetValue(),
                          generalSettings.grid_height().GetValue());
   }

   const auto defaultTimeZone = p->activeMap_->GetDefaultTimeZone();

   // Configure Alert Dock
   p->alertDockWidget_ = new ui::AlertDockWidget(this);
   addDockWidget(Qt::BottomDockWidgetArea, p->alertDockWidget_);

   // Configure GFS Sounding Dock
   p->soundingPanel_ = new ui::SoundingPanel(this);
   addDockWidget(Qt::RightDockWidgetArea, p->soundingPanel_);
   p->soundingPanel_->hide();

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

   // Add GFS Sounding menu action
   auto* soundingAction = ui->menuView->addAction(tr("GFS &Sounding"));
   soundingAction->setCheckable(true);
   soundingAction->setChecked(false);
   QObject::connect(soundingAction,
                    &QAction::toggled,
                    this,
                    [this](bool checked)
                    { p->soundingPanel_->setVisible(checked); });
   QObject::connect(p->soundingPanel_,
                    &QDockWidget::visibilityChanged,
                    soundingAction,
                    &QAction::setChecked);
   QObject::connect(
      p->soundingPanel_,
      &QDockWidget::visibilityChanged,
      this,
      [this](bool visible)
      {
         QTimer::singleShot(
            0,
            this,
            [this, visible]()
            {
               if (visible)
               {
                  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
               }
               else
               {
                  setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
               }
            });
      });

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

   // Radar Presets
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(ui->presetLabel);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(ui->presetComboBox);

   QWidget*     presetButtonsWidget = new QWidget(this);
   QHBoxLayout* presetButtonsLayout = new QHBoxLayout(presetButtonsWidget);
   presetButtonsLayout->setContentsMargins(0, 0, 0, 0);
   presetButtonsLayout->addWidget(ui->savePresetButton);
   presetButtonsLayout->addWidget(ui->loadPresetButton);
   presetButtonsLayout->addWidget(ui->renamePresetButton);
   presetButtonsLayout->addWidget(ui->deletePresetButton);
   p->mapSettingsGroup_->GetContentsLayout()->addWidget(presetButtonsWidget);

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

   // SPC Convective Outlook
   p->spcOutlookGroup_ =
      new ui::CollapsibleGroup(tr("SPC Convective Outlooks"), this);
   auto* spcLayout =
      qobject_cast<QVBoxLayout*>(p->spcOutlookGroup_->GetContentsLayout());

   // Day selector
   auto* dayLabel  = new QLabel(tr("Day:"), this);
   p->spcDayCombo_ = new QComboBox(this);
   p->spcDayCombo_->addItem(tr("Day 1"));
   p->spcDayCombo_->addItem(tr("Day 2"));
   p->spcDayCombo_->addItem(tr("Day 3"));

   // Product selector
   auto* productLabel  = new QLabel(tr("Product:"), this);
   p->spcProductCombo_ = new QComboBox(this);

   // Opacity slider
   p->spcOpacitySlider_                = new QSlider(Qt::Horizontal, this);
   static constexpr int kSpcOpacityMax = 100;
   p->spcOpacitySlider_->setRange(0, kSpcOpacityMax);
   p->spcOpacitySlider_->setValue(
      settings::SpcOutlookSettings::Instance().opacity().GetValue());

   // Auto-refresh
   p->spcAutoRefreshCheck_ = new QCheckBox(tr("Auto-refresh"), this);
   p->spcAutoRefreshCheck_->setChecked(
      settings::SpcOutlookSettings::Instance().auto_refresh().GetValue());

   // Layout
   auto* dayRow = new QHBoxLayout();
   dayRow->addWidget(dayLabel);
   dayRow->addWidget(p->spcDayCombo_, 1);
   spcLayout->addLayout(dayRow);

   auto* productRow = new QHBoxLayout();
   productRow->addWidget(productLabel);
   productRow->addWidget(p->spcProductCombo_, 1);
   spcLayout->addLayout(productRow);

   spcLayout->addWidget(p->spcOpacitySlider_);
   spcLayout->addWidget(p->spcAutoRefreshCheck_);

   ui->radarToolboxScrollAreaContents->layout()->addWidget(p->spcOutlookGroup_);

   auto updateProductCombo = [this](scwx::spc::OutlookDay day)
   {
      p->spcProductCombo_->clear();
      switch (day)
      {
      case scwx::spc::OutlookDay::Day1:
      case scwx::spc::OutlookDay::Day2:
         p->spcProductCombo_->addItem(tr("Categorical"));
         p->spcProductCombo_->addItem(tr("Tornado"));
         p->spcProductCombo_->addItem(tr("Wind"));
         p->spcProductCombo_->addItem(tr("Hail"));
         break;
      case scwx::spc::OutlookDay::Day3:
         p->spcProductCombo_->addItem(tr("Categorical"));
         p->spcProductCombo_->addItem(tr("Probabilistic"));
         p->spcProductCombo_->addItem(tr("Significant Probabilistic"));
         break;
      default:
         break;
      }
   };

   auto getSelectedDay = [this]() -> scwx::spc::OutlookDay
   {
      switch (p->spcDayCombo_->currentIndex())
      {
      case 0:
         return scwx::spc::OutlookDay::Day1;
      case 1:
         return scwx::spc::OutlookDay::Day2;
      case 2:
         return scwx::spc::OutlookDay::Day3;
      default:
         return scwx::spc::OutlookDay::Day1;
      }
   };

   auto getSelectedProduct = [this]() -> scwx::spc::OutlookProduct
   {
      int dayIdx  = p->spcDayCombo_->currentIndex();
      int prodIdx = p->spcProductCombo_->currentIndex();

      static constexpr int kCategoricalIdx = 0;
      static constexpr int kTornadoIdx     = 1;
      static constexpr int kWindIdx        = 2;
      static constexpr int kHailIdx        = 3;

      // Day 3: Categorical=0, Probabilistic=1, SigProbabilistic=2
      if (dayIdx == 2)
      {
         switch (prodIdx)
         {
         case kCategoricalIdx:
            return scwx::spc::OutlookProduct::Categorical;
         case 1:
            return scwx::spc::OutlookProduct::Probabilistic;
         case 2:
            return scwx::spc::OutlookProduct::SignificantProbabilistic;
         default:
            return scwx::spc::OutlookProduct::Categorical;
         }
      }

      // Day 1/2: Categorical=0, Tornado=1, Wind=2, Hail=3
      switch (prodIdx)
      {
      case kCategoricalIdx:
         return scwx::spc::OutlookProduct::Categorical;
      case kTornadoIdx:
         return scwx::spc::OutlookProduct::Tornado;
      case kWindIdx:
         return scwx::spc::OutlookProduct::Wind;
      case kHailIdx:
         return scwx::spc::OutlookProduct::Hail;
      default:
         return scwx::spc::OutlookProduct::Categorical;
      }
   };

   // Day changed -> update products, trigger fetch
   using IndexSignal = void (QComboBox::*)(int);
   QObject::connect(
      p->spcDayCombo_,
      static_cast<IndexSignal>(&QComboBox::currentIndexChanged),
      [updateProductCombo, getSelectedDay, getSelectedProduct]()
      {
         auto day = getSelectedDay();
         updateProductCombo(day);

         auto& settings = settings::SpcOutlookSettings::Instance();
         settings.selected_day().StageValue(scwx::spc::GetOutlookDayName(day));
         settings.selected_product().StageValue(
            scwx::spc::GetOutlookProductName(getSelectedProduct()));

         auto& manager = manager::SpcOutlookManager::Instance();
         manager.SelectDay(day);
         manager.SelectProduct(getSelectedProduct());
      });

   // Product changed -> trigger fetch
   QObject::connect(p->spcProductCombo_,
                    static_cast<IndexSignal>(&QComboBox::currentIndexChanged),
                    [getSelectedProduct]()
                    {
                       auto product = getSelectedProduct();

                       auto& settings =
                          settings::SpcOutlookSettings::Instance();
                       settings.selected_product().StageValue(
                          scwx::spc::GetOutlookProductName(product));

                       auto& manager = manager::SpcOutlookManager::Instance();
                       manager.SelectProduct(product);
                    });

   // Opacity changed
   QObject::connect(p->spcOpacitySlider_,
                    static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
                    [](int value)
                    {
                       auto& opacitySetting =
                          settings::SpcOutlookSettings::Instance().opacity();
                       opacitySetting.StageValue(value);
                       opacitySetting.Commit();
                       manager::SpcOutlookManager::Instance().SetOpacity(value);
                    });

   // Auto-refresh changed
   QObject::connect(
      p->spcAutoRefreshCheck_,
      &QCheckBox::checkStateChanged,
      [](Qt::CheckState state)
      {
         bool enabled = (state == Qt::CheckState::Checked);
         settings::SpcOutlookSettings::Instance().auto_refresh().StageValue(
            enabled);
         manager::SpcOutlookManager::Instance().SetAutoRefresh(enabled);
      });

   // Populate products for default day
   updateProductCombo(getSelectedDay());

   // Initial fetch
   manager::SpcOutlookManager::Instance().RefreshNow();
   manager::SpcOutlookManager::Instance().SetOpacity(
      p->spcOpacitySlider_->value());
   manager::SpcOutlookManager::Instance().SetAutoRefresh(
      p->spcAutoRefreshCheck_->isChecked());

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

   if (presetIndex.has_value())
   {
      auto& preset = presetSettings.presets().at(*presetIndex);
      for (size_t i = 0; i < p->maps_.size(); i++)
      {
         p->SelectRadarProduct(
            p->maps_.at(i),
            common::GetRadarProductGroup(preset.products[i].group),
            preset.products[i].name,
            0);
      }
   }
   else
   {
      for (size_t i = 0; i < p->maps_.size(); i++)
      {
         p->SelectRadarProduct(
            p->maps_.at(i),
            common::GetRadarProductGroup(
               mapSettings.radar_product_group(i).GetValue()),
            mapSettings.radar_product(i).GetValue(),
            0);
      }
   }

   p->PopulatePresetComboBox();
   p->PopulateMapStyles();
   p->ConfigureMapStyles();
   p->ConfigureUiSettings();
   p->ConnectMapSignals();
   p->ConnectAnimationSignals();
   p->ConnectOtherSignals();
   p->HandleFocusChange(p->activeMap_);
   p->AsyncSetup();

   // Install event filter to detect user dock resize
   ui->radarToolboxDock->installEventFilter(this);

   // Debounce timer for saving dock width after user resize
   p->dockWidthSaveTimer_.setSingleShot(true);
   connect(&p->dockWidthSaveTimer_,
           &QTimer::timeout,
           [this]()
           {
              auto& uiSettings = settings::UiSettings::Instance();
              int   width      = ui->radarToolboxDock->width();
              uiSettings.radar_toolbox_dock_width().StageValue(width);
           });

   // Apply saved dock width
   auto&        uiSettings = settings::UiSettings::Instance();
   std::int64_t dockWidth  = uiSettings.radar_toolbox_dock_width().GetValue();
   resizeDocks(
      {ui->radarToolboxDock}, {static_cast<int>(dockWidth)}, Qt::Horizontal);

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
      if (ev->key() == Qt::Key_Control || ev->key() == Qt::Key_Shift ||
          ev->key() == Qt::Key_Alt || ev->key() == Qt::Key_Meta)
      {
         for (auto& map : p->maps_)
         {
            map->update();
         }
      }
      else
      {
         p->activeMap_->update();
      }
      ev->accept();
   }
}

void MainWindow::keyReleaseEvent(QKeyEvent* ev)
{
   if (p->hotkeyManager_->HandleKeyRelease(ev))
   {
      if (ev->key() == Qt::Key_Control || ev->key() == Qt::Key_Shift ||
          ev->key() == Qt::Key_Alt || ev->key() == Qt::Key_Meta)
      {
         for (auto& map : p->maps_)
         {
            map->update();
         }
      }
      else
      {
         p->activeMap_->update();
      }
      ev->accept();
   }
}

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   if (p->firstShow_)
   {
      auto& uiSettings = settings::UiSettings::Instance();

      // restore the geometry state
      const std::string uiGeometry = uiSettings.main_ui_geometry().GetValue();
      restoreGeometry(
         QByteArray::fromBase64(QByteArray::fromStdString(uiGeometry)));

      // restore the UI state
      const std::string uiState  = uiSettings.main_ui_state().GetValue();
      bool              restored = restoreState(
         QByteArray::fromBase64(QByteArray::fromStdString(uiState)));

      if (!restored)
      {
         logger_->warn("Failed to restore UI state");
      }

      // Apply saved dock width (overrides restoreState proportions)
      std::int64_t dockWidth = uiSettings.radar_toolbox_dock_width().GetValue();
      p->applyingDockWidth_  = true;
      resizeDocks(
         {ui->radarToolboxDock}, {static_cast<int>(dockWidth)}, Qt::Horizontal);
      p->applyingDockWidth_ = false;

      p->firstShow_ = false;
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

void MainWindow::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);

   auto&        uiSettings = settings::UiSettings::Instance();
   std::int64_t dockWidth  = uiSettings.radar_toolbox_dock_width().GetValue();
   p->applyingDockWidth_   = true;
   resizeDocks(
      {ui->radarToolboxDock}, {static_cast<int>(dockWidth)}, Qt::Horizontal);
   p->applyingDockWidth_ = false;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
   if (obj == ui->radarToolboxDock && event->type() == QEvent::Resize &&
       !p->applyingDockWidth_)
   {
      static constexpr int kDockWidthDebounceMs = 500;
      p->dockWidthSaveTimer_.start(kDockWidthDebounceMs);
   }
   return QMainWindow::eventFilter(obj, event);
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
      [this, currentMap](const QString& file)
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
            [this, currentMap, file](
               const std::shared_ptr<request::NexradFileRequest>& request)
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
                  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                  auto* messageBox = new QMessageBox(this);
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
   p->settingsDialog_->RefreshWidgets();
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
                     // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                     auto* messageBox = new QMessageBox(this);
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
      if (map->isVisible())
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

void MainWindowImpl::InitializeGlContext()
{
   if (!glContext_)
   {
      glContext_ = std::make_shared<gl::GlContext>();
   }
}

void MainWindowImpl::CreateAllMapWidgets()
{
   maps_.resize(types::kMapCount_, nullptr);

   for (std::size_t i = 0; i < types::kMapCount_; ++i)
   {
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Owned by parent
      maps_[i] = new map::MapWidget(i, settings_, glContext_);
      maps_[i]->setParent(mapContainer_);
      maps_[i]->setVisible(false);

      ConnectMapSignalsForWidget(maps_[i]);
      ConnectAnimationSignalsForWidget(maps_[i], i);
   }
}

void MainWindowImpl::RebuildMapLayout(std::int64_t gridWidth,
                                      std::int64_t gridHeight,
                                      QList<int>   rowSizes,
                                      QList<int>   columnSizes)
{
   const std::int64_t mapCount = gridWidth * gridHeight;

   QLayout* centralLayout = mainWindow_->ui->centralwidget->layout();
   if (centralLayout == nullptr)
   {
      mainWindow_->ui->centralwidget->setLayout(new QVBoxLayout());
      centralLayout = mainWindow_->ui->centralwidget->layout();
   }

   // Find the old vertical splitter and save its sizes as fallback
   QSplitter* oldSplitter = nullptr;
   QList<int> oldSplitterSizes {};
   for (int i = 0; i < centralLayout->count(); ++i)
   {
      QLayoutItem* item = centralLayout->itemAt(i);
      if (item != nullptr && item->widget() != nullptr)
      {
         oldSplitter = qobject_cast<QSplitter*>(item->widget());
         if (oldSplitter != nullptr)
         {
            oldSplitterSizes = oldSplitter->sizes();
            centralLayout->removeWidget(oldSplitter);
            break;
         }
      }
   }

   timelineManager_->SetMapCount(mapCount);

   // Move excess MapWidgets to hidden container
   for (std::size_t i = mapCount; i < maps_.size(); ++i)
   {
      if (maps_[i] != nullptr)
      {
         maps_[i]->setParent(mapContainer_);
         maps_[i]->setVisible(false);
      }
   }

   // Build new splitter layout
   QSplitter* vs = new QSplitter(Qt::Vertical);
   vs->setHandleWidth(1);

   std::size_t mapIndex = 0;

   auto MoveSplitter = [this, vs](int /*pos*/, int /*index*/)
   {
      auto* s = qobject_cast<QSplitter*>(sender());

      auto sizes = s->sizes();
      for (QSplitter* hs : vs->findChildren<QSplitter*>())
      {
         hs->setSizes(sizes);
      }
   };

   for (std::int64_t y = 0; y < gridHeight; y++)
   {
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      auto* hs = new QSplitter(vs);
      hs->setHandleWidth(1);

      for (std::int64_t x = 0; x < gridWidth; x++, mapIndex++)
      {
         maps_[mapIndex]->setVisible(true);
         hs->addWidget(maps_[mapIndex]);
      }

      connect(hs, &QSplitter::splitterMoved, this, MoveSplitter);
   }

   // Restore row sizes after all children are added
   if (!rowSizes.isEmpty() &&
       rowSizes.size() == static_cast<qsizetype>(gridHeight))
   {
      vs->setSizes(rowSizes);
   }
   else if (!oldSplitterSizes.isEmpty())
   {
      QList<int> fallbackSizes;
      if (oldSplitterSizes.size() == static_cast<qsizetype>(gridHeight))
      {
         fallbackSizes = oldSplitterSizes;
      }
      else
      {
         int totalOld = 0;
         for (int s : oldSplitterSizes)
            totalOld += s;
         int perRow = totalOld / static_cast<int>(gridHeight);
         int rem    = totalOld % static_cast<int>(gridHeight);
         for (int i = 0; i < gridHeight; i++)
            fallbackSizes.append(perRow + (i < rem ? 1 : 0));
      }
      vs->setSizes(fallbackSizes);
   }

   // Restore column sizes after all children are added
   if (!columnSizes.isEmpty() &&
       columnSizes.size() == static_cast<qsizetype>(gridWidth))
   {
      for (QSplitter* hs :
           vs->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly))
      {
         hs->setSizes(columnSizes);
      }
   }

   centralLayout->addWidget(vs);

   // Hide and defer destruction of old splitter
   if (oldSplitter != nullptr)
   {
      oldSplitter->hide();
      oldSplitter->deleteLater();
   }

   if (mapCount > 0)
   {
      SetActiveMap(maps_.at(0));
   }
   else
   {
      SetActiveMap(nullptr);
   }
}

void MainWindowImpl::PopulatePresetComboBox()
{
   auto&      presetSettings = settings::RadarPresetSettings::Instance();
   QComboBox* combo          = mainWindow_->ui->presetComboBox;

   QSignalBlocker blocker(combo);

   combo->clear();
   int selectIndex = -1;

   // Add preset names
   for (const auto& preset : presetSettings.presets())
   {
      combo->addItem(QString::fromStdString(preset.name));
      if (preset.name == presetSettings.active_preset())
      {
         selectIndex = combo->count() - 1;
      }
   }

   // Add "Custom" as the last entry
   combo->addItem(tr("Custom"));

   if (selectIndex >= 0)
   {
      combo->setCurrentIndex(selectIndex);
   }
   else
   {
      combo->setCurrentIndex(combo->count() - 1);
   }
}

void MainWindowImpl::OnSavePreset()
{
   auto& presetSettings  = settings::RadarPresetSettings::Instance();
   auto& generalSettings = settings::GeneralSettings::Instance();

   QComboBox* combo       = mainWindow_->ui->presetComboBox;
   QString    currentText = combo->currentText();

   std::string presetName;
   bool        overwriteExisting = false;

   // If a preset name is selected (not "Custom"), ask to overwrite or save as
   // new
   if (currentText != tr("Custom"))
   {
      QString selectedName = currentText;
      auto    result       = QMessageBox::question(
         mainWindow_,
         tr("Save Preset"),
         tr("Overwrite preset \"%1\"?").arg(selectedName),
         QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

      if (result == QMessageBox::Cancel)
      {
         return;
      }

      if (result == QMessageBox::Yes)
      {
         presetName        = selectedName.toStdString();
         overwriteExisting = true;
      }
   }

   // If we still need a name, prompt for one
   if (presetName.empty())
   {
      bool    ok;
      QString name = QInputDialog::getText(mainWindow_,
                                           tr("Save Preset"),
                                           tr("Preset name:"),
                                           QLineEdit::Normal,
                                           {},
                                           &ok);
      if (!ok || name.isEmpty())
      {
         return;
      }

      presetName = name.toStdString();

      // Check uniqueness
      if (presetSettings.FindPresetIndex(presetName).has_value())
      {
         QMessageBox::warning(
            mainWindow_,
            tr("Duplicate Preset"),
            tr("A preset with the name \"%1\" already exists.")
               .arg(QString::fromStdString(presetName)));
         return;
      }
   }

   // Build the preset from current state
   settings::RadarPresetSettings::Preset preset;
   preset.name       = presetName;
   preset.gridWidth  = generalSettings.grid_width().GetValue();
   preset.gridHeight = generalSettings.grid_height().GetValue();

   // Capture current splitter sizes
   {
      QLayout* cl = mainWindow_->ui->centralwidget->layout();
      if (cl != nullptr)
      {
         for (int i = 0; i < cl->count(); ++i)
         {
            QLayoutItem* item = cl->itemAt(i);
            QSplitter*   vs   = (item != nullptr) ?
                                   qobject_cast<QSplitter*>(item->widget()) :
                                   nullptr;
            if (vs != nullptr)
            {
               // Row sizes (vertical splitter)
               {
                  QList<int> sizes = vs->sizes();
                  preset.rowSizes.assign(sizes.begin(), sizes.end());
               }
               // Column sizes (first horizontal splitter — all synced)
               QList<QSplitter*> hss = vs->findChildren<QSplitter*>(
                  QString(), Qt::FindDirectChildrenOnly);
               if (!hss.isEmpty())
               {
                  QList<int> sizes = hss.first()->sizes();
                  preset.columnSizes.assign(sizes.begin(), sizes.end());
               }
               break;
            }
         }
      }
   }

   for (std::size_t i = 0; i < settings::RadarPresetSettings::kMapCount_; i++)
   {
      if (i < maps_.size())
      {
         preset.products[i].group =
            common::GetRadarProductGroupName(maps_[i]->GetRadarProductGroup());
         preset.products[i].name = maps_[i]->GetRadarProductName();
      }
      else
      {
         preset.products[i].group = "L3";
         preset.products[i].name  = "N0B";
      }
   }

   // Store the preset
   if (overwriteExisting)
   {
      auto idx = presetSettings.FindPresetIndex(presetName);
      if (idx.has_value())
      {
         presetSettings.presets()[idx.value()] = preset;
      }
   }
   else
   {
      presetSettings.presets().push_back(preset);
   }

   presetSettings.set_active_preset(presetName);
   manager::SettingsManager::Instance().SaveSettings();

   PopulatePresetComboBox();
}

void MainWindowImpl::OnLoadPreset()
{
   QComboBox* combo       = mainWindow_->ui->presetComboBox;
   QString    currentText = combo->currentText();

   // Don't load "Custom"
   if (currentText == tr("Custom") || combo->currentIndex() < 0)
   {
      return;
   }

   std::string presetName     = currentText.toStdString();
   auto&       presetSettings = settings::RadarPresetSettings::Instance();
   auto        idx            = presetSettings.FindPresetIndex(presetName);

   if (!idx.has_value())
   {
      return;
   }

   auto& preset = presetSettings.presets().at(idx.value());

   // Set grid dimensions without triggering changed signals
   applyingGridChange_   = true;
   auto& generalSettings = settings::GeneralSettings::Instance();
   generalSettings.grid_width().SetValue(preset.gridWidth);
   generalSettings.grid_height().SetValue(preset.gridHeight);

   // Rebuild layout with new dimensions and saved splitter sizes
   {
      QList<int> rowSizes, columnSizes;
      for (int s : preset.rowSizes)
         rowSizes.append(s);
      for (int s : preset.columnSizes)
         columnSizes.append(s);
      RebuildMapLayout(
         preset.gridWidth, preset.gridHeight, rowSizes, columnSizes);
   }
   applyingGridChange_ = false;

   // Apply products to each pane
   auto& mapSettings = settings::MapSettings::Instance();
   for (std::size_t i = 0; i < maps_.size(); i++)
   {
      const std::string& group = preset.products[i].group;
      const std::string& name  = preset.products[i].name;
      SelectRadarProduct(
         maps_[i], common::GetRadarProductGroup(group), name, 0);

      // Stage values so they're saved on shutdown
      if (i < settings::RadarPresetSettings::kMapCount_)
      {
         mapSettings.radar_product_group(i).StageValue(group);
         mapSettings.radar_product(i).StageValue(name);
      }
   }

   presetSettings.set_active_preset(presetName);
   manager::SettingsManager::Instance().SaveSettings();

   PopulatePresetComboBox();

   // Update layer dialog columns for the new map count
   if (layerDialog_ != nullptr)
   {
      layerDialog_->UpdateMapDisplayColumns();
   }
}

void MainWindowImpl::OnRenamePreset()
{
   QComboBox* combo       = mainWindow_->ui->presetComboBox;
   QString    currentText = combo->currentText();

   if (currentText == tr("Custom") || combo->currentIndex() < 0)
   {
      return;
   }

   std::string oldName        = currentText.toStdString();
   auto&       presetSettings = settings::RadarPresetSettings::Instance();

   bool    ok;
   QString newName = QInputDialog::getText(mainWindow_,
                                           tr("Rename Preset"),
                                           tr("New name:"),
                                           QLineEdit::Normal,
                                           currentText,
                                           &ok);
   if (!ok || newName.isEmpty())
   {
      return;
   }

   std::string newNameStr = newName.toStdString();

   // Check uniqueness
   if (newNameStr != oldName &&
       presetSettings.FindPresetIndex(newNameStr).has_value())
   {
      QMessageBox::warning(
         mainWindow_,
         tr("Duplicate Preset"),
         tr("A preset with the name \"%1\" already exists.").arg(newName));
      return;
   }

   auto idx = presetSettings.FindPresetIndex(oldName);
   if (idx.has_value())
   {
      presetSettings.presets()[idx.value()].name = newNameStr;

      if (presetSettings.active_preset() == oldName)
      {
         presetSettings.set_active_preset(newNameStr);
      }

      manager::SettingsManager::Instance().SaveSettings();
   }

   PopulatePresetComboBox();
}

void MainWindowImpl::OnDeletePreset()
{
   QComboBox* combo       = mainWindow_->ui->presetComboBox;
   QString    currentText = combo->currentText();

   if (currentText == tr("Custom") || combo->currentIndex() < 0)
   {
      return;
   }

   std::string presetName     = currentText.toStdString();
   auto&       presetSettings = settings::RadarPresetSettings::Instance();

   auto result =
      QMessageBox::question(mainWindow_,
                            tr("Delete Preset"),
                            tr("Delete preset \"%1\"?").arg(currentText),
                            QMessageBox::Yes | QMessageBox::No);

   if (result != QMessageBox::Yes)
   {
      return;
   }

   auto idx = presetSettings.FindPresetIndex(presetName);
   if (idx.has_value())
   {
      presetSettings.presets().erase(presetSettings.presets().begin() +
                                     static_cast<std::ptrdiff_t>(idx.value()));

      if (presetSettings.active_preset() == presetName)
      {
         presetSettings.set_active_preset(std::string());
      }

      manager::SettingsManager::Instance().SaveSettings();
   }

   PopulatePresetComboBox();
}

void MainWindowImpl::ClearActivePreset()
{
   settings::RadarPresetSettings::Instance().set_active_preset(std::string());
   PopulatePresetComboBox();
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
   spcOutlookGroup_->SetExpanded(uiSettings.spc_outlook_expanded().GetValue());

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
   connect(spcOutlookGroup_,
           &ui::CollapsibleGroup::StateChanged,
           [&](bool expanded)
           { uiSettings.spc_outlook_expanded().StageValue(expanded); });
}

void MainWindowImpl::ConnectMapSignals()
{
   for (const auto& mapWidget : maps_)
   {
      ConnectMapSignalsForWidget(mapWidget);
   }
}

void MainWindowImpl::ConnectMapSignalsForWidget(map::MapWidget* mapWidget)
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
      [this, mapWidget](double latitude, double longitude)
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
           [this, mapWidget](const std::string& mapStyle)
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
      [this, mapWidget]()
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
      [this, mapWidget]()
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

   connect(mapWidget,
           &map::MapWidget::MapClicked,
           this,
           [this](common::Coordinate coordinate)
           {
              if (!selectingSoundingPoint_)
              {
                 return;
              }
              selectingSoundingPoint_ = false;
              activeMap_->setCursor(Qt::ArrowCursor);
              soundingPanel_->SetLocation(coordinate.latitude_,
                                          coordinate.longitude_);
              soundingPanel_->show();
              soundingPanel_->raise();
           });
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
      ConnectAnimationSignalsForWidget(maps_[i], i);
   }
}

void MainWindowImpl::ConnectAnimationSignalsForWidget(map::MapWidget* mapWidget,
                                                      std::size_t     index)
{
   connect(mapWidget,
           &map::MapWidget::RadarSweepUpdated,
           timelineManager_.get(),
           [=, this]() { timelineManager_->ReceiveRadarSweepUpdated(index); });
   connect(mapWidget,
           &map::MapWidget::RadarSweepNotUpdated,
           timelineManager_.get(),
           [=, this](types::NoUpdateReason reason)
           { timelineManager_->ReceiveRadarSweepNotUpdated(index, reason); });
   connect(mapWidget,
           &map::MapWidget::WidgetPainted,
           timelineManager_.get(),
           [=, this]() { timelineManager_->ReceiveMapWidgetPainted(index); });
   connect(mapWidget,
           &map::MapWidget::RadarSiteRequested,
           this,
           [this](const std::string& id, bool updateCoordinates)
           {
              for (map::MapWidget* map : maps_)
              {
                 if (map->isVisible())
                    map->SelectRadarSite(id, updateCoordinates);
              }

              UpdateRadarSite();
              UpdateAvailableLevel3Products();
              UpdateRadarProductSettings();
           });
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

   // Radar Preset connections
   connect(mainWindow_->ui->presetComboBox,
           &QComboBox::currentIndexChanged,
           mainWindow_,
           [this]() { /* selection change tracked by Load button */ });
   connect(mainWindow_->ui->savePresetButton,
           &QAbstractButton::clicked,
           mainWindow_,
           [this]() { OnSavePreset(); });
   connect(mainWindow_->ui->loadPresetButton,
           &QAbstractButton::clicked,
           mainWindow_,
           [this]() { OnLoadPreset(); });
   connect(mainWindow_->ui->renamePresetButton,
           &QAbstractButton::clicked,
           mainWindow_,
           [this]() { OnRenamePreset(); });
   connect(mainWindow_->ui->deletePresetButton,
           &QAbstractButton::clicked,
           mainWindow_,
           [this]() { OnDeletePreset(); });

   connect(level2ProductsWidget_,
           &ui::Level2ProductsWidget::RadarProductSelected,
           mainWindow_,
           [this](common::RadarProductGroup group,
                  const std::string&        productName,
                  int16_t                   productCode)
           {
              SelectRadarProduct(activeMap_, group, productName, productCode);
              ClearActivePreset();
           });
   connect(level3ProductsWidget_,
           &ui::Level3ProductsWidget::RadarProductSelected,
           mainWindow_,
           [this](common::RadarProductGroup group,
                  const std::string&        productName,
                  int16_t                   productCode)
           {
              SelectRadarProduct(activeMap_, group, productName, productCode);
              ClearActivePreset();
           });
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
                 if (map->isVisible())
                    map->SelectRadarSite(selectedRadarSite);
              }

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
   static constexpr int kClockTimerIntervalMs = 1000;
   clockTimer_.start(kClockTimerIntervalMs);

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

   // Deferred grid rebuild — fires once after both width/height have been
   // committed
   gridRebuildTimer_.setSingleShot(true);
   connect(&gridRebuildTimer_,
           &QTimer::timeout,
           [this]()
           {
              auto& gs = settings::GeneralSettings::Instance();
              RebuildMapLayout(gs.grid_width().GetValue(),
                               gs.grid_height().GetValue());

              ClearActivePreset();

              if (layerDialog_ != nullptr)
              {
                 layerDialog_->UpdateMapDisplayColumns();
              }
           });

   gridWidthConnection_ = generalSettings.grid_width().changed_signal().connect(
      [this](auto&&...) mutable
      {
         if (applyingGridChange_)
            return;
         if (!gridRebuildTimer_.isActive())
         {
            gridRebuildTimer_.start(0);
         }
      });
   gridHeightConnection_ =
      generalSettings.grid_height().changed_signal().connect(
         [this](auto&&...) mutable
         {
            if (applyingGridChange_)
               return;
            if (!gridRebuildTimer_.isActive())
            {
               gridRebuildTimer_.start(0);
            }
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

   // Connect sounding panel point selection
   connect(soundingPanel_,
           &ui::SoundingPanel::PointSelectionStarted,
           this,
           [this]()
           {
              selectingSoundingPoint_ = true;
              activeMap_->setCursor(Qt::CrossCursor);
           });

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
                 if (map->isVisible())
                    map->SelectRadarSite(siteId);
              }

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

} // namespace scwx::qt::main

#include "main_window.moc"
