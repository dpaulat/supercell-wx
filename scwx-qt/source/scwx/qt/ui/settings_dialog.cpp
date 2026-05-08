#include "settings_dialog.hpp"
#include "ui_settings_dialog.h"

#include <scwx/awips/phenomenon.hpp>
#include <scwx/common/color_table.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/main/theme.hpp>
#include <scwx/qt/manager/media_manager.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/settings/audio_settings.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/alert_types.hpp>
#include <scwx/qt/types/font_types.hpp>
#include <scwx/qt/types/location_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/types/text_types.hpp>
#include <scwx/qt/types/time_types.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/qt/ui/county_dialog.hpp>
#include <scwx/qt/ui/custom_layer_dialog.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
#include <scwx/qt/ui/serial_port_dialog.hpp>
#include <scwx/qt/ui/settings/alert_palette_settings_widget.hpp>
#include <scwx/qt/ui/settings/hotkey_settings_widget.hpp>
#include <scwx/qt/ui/settings/radar_site_status_palette_settings_widget.hpp>
#include <scwx/qt/ui/settings/unit_settings_widget.hpp>
#include <scwx/qt/ui/wfo_dialog.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/qt/util/file.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFontDialog>
#include <QGeoPositionInfo>
#include <QPushButton>
#include <QStandardItemModel>
#include <QToolButton>
#include <utility>

#define QT6CT_LIBRARY
#include <qt6ct-common/qt6ct.h>
#include <qt6ct/paletteeditdialog.h>
#undef QT6CT_LIBRARY

namespace scwx::qt::ui
{

// Extensive usage of "new" with Qt-managed objects
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

static const std::string logPrefix_ = "scwx::qt::ui::settings_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

struct ColorTableConversions
{
   uint16_t rangeMin {0u};
   uint16_t rangeMax {255u};
   float    offset {0.0f};
   float    scale {1.0f};
};

static const std::array<std::pair<std::string, std::string>, 16>
   kColorTableTypes_ {std::pair {"BR", "BR"},
                      std::pair {"BV", "BV"},
                      std::pair {"SW", "SW"},
                      std::pair {"ZDR", "ZDR"},
                      std::pair {"PHI2", "KDP (L2)"},
                      std::pair {"CC", "CC"},
                      std::pair {"DOD", "DOD"},
                      std::pair {"DSD", "DSD"},
                      std::pair {"ET", "ET"},
                      std::pair {"HC", "HC"},
                      std::pair {"OHP", "OHP"},
                      std::pair {"PHI3", "KDP"},
                      std::pair {"SRV", "SRV"},
                      std::pair {"STP", "STP"},
                      std::pair {"VIL", "VIL"},
                      std::pair {"???", "Default"}};

// Color table conversions for display, roughly based upon:
// - ICD for RDA-RPG: Data Moment Characteristics and Conversion for Data Names
// - ICD for the RPG to Class 1 User
static const std::unordered_map<std::string, ColorTableConversions>
   kColorTableConversions_ {{"BR", {0u, 255u, 66.0f, 2.0f}},
                            {"BV", {0u, 255u, 129.0f, 2.0f}},
                            {"SW", {0u, 255u, 129.0f, 2.0f}},
                            {"ZDR", {0u, 1058u, 418.0f, 32.0f}},
                            {"PHI2", {0u, 1023u, 2.0f, 2.8361f}},
                            {"CC", {0u, 255u, -60.5f, 300.0f}},
                            {"DOD", {0u, 255u, 128.0f, 1.5f}},
                            {"DSD", {0u, 255u, 128.0f, 1.5f}},
                            {"ET", {0u, 255u, 2.0f, 1.0f}},
                            {"HC", {10u, 160u, 0.0f, 1.0f}},
                            {"OHP", {0u, 255u, 0.0f, 2.5f}},
                            {"PHI3", {0u, 255u, 43.0f, 20.0f}},
                            {"SRV", {0u, 255u, 128.0f, 2.0f}},
                            {"STP", {0u, 255u, 0.0f, 1.25f}},
                            {"VIL", {0u, 255u, 1.0f, 2.5f}},
                            {"???", {0u, 15u, 0.0f, 1.0f}}};

class SettingsDialogImpl
{
public:
   explicit SettingsDialogImpl(SettingsDialog*      self,
                               QMapLibre::Settings& mapSettings) :
       self_ {self},
       radarSiteDialog_ {new RadarSiteDialog(self)},
       alertAudioRadarSiteDialog_ {new RadarSiteDialog(self)},
       gpsSourceDialog_ {new SerialPortDialog(self)},
       countyDialog_ {new CountyDialog(self)},
       wfoDialog_ {new WFODialog(self)},
       fontDialog_ {new QFontDialog(self)},
       mapSettings_ {mapSettings},
       fontCategoryModel_ {new QStandardItemModel(self)},
       settings_ {std::initializer_list<settings::SettingsInterfaceBase*> {
          &defaultRadarSite_,
          &gridWidth_,
          &gridHeight_,
          &mapProvider_,
          &mapboxApiKey_,
          &mapTilerApiKey_,
          &theme_,
          &themeFile_,
          &defaultAlertAction_,
          &clockFormat_,
          &customStyleDrawLayer_,
          &customStyleUrl_,
          &screenCaptureFolder_,
          &screenCaptureName_,
          &defaultTimeZone_,
          &positioningPlugin_,
          &nmeaBaudRate_,
          &nmeaSource_,
          &warningsProvider_,
          &radarSiteThreshold_,
          &antiAliasingEnabled_,
          &autoNavigateToWsr88dOnly_,
          &centerOnRadarSelection_,
          &screenCaptureOnRefresh_,
          &showMapAttribution_,
          &showMapCenter_,
          &showMapLogo_,
          &showSmoothedRangeFolding_,
          &updateNotificationsEnabled_,
          &cursorIconAlwaysOn_,
          &cursorIconScale_,
          &debugEnabled_,
          &alertAudioSoundFile_,
          &alertAudioLocationMethod_,
          &alertAudioLatitude_,
          &alertAudioLongitude_,
          &alertAudioRadarSite_,
          &alertAudioRadius_,
          &alertAudioCounty_,
          &alertAudioWFO_,
          &masterVolume_,
          &hoverTextWrap_,
          &tooltipMethod_,
          &placefileTextDropShadowEnabled_,
          &radarSiteHoverTextEnabled_}}
   {
      // Configure default alert phenomena colors
      auto& paletteSettings = settings::PaletteSettings::Instance();
      int   index           = 0;

      for (auto& phenomenon : settings::PaletteSettings::alert_phenomena())
      {
         QColorDialog::setCustomColor(
            index++,
            QColor(QString::fromStdString(
               paletteSettings.alert_color(phenomenon, true).GetDefault())));
         QColorDialog::setCustomColor(
            index++,
            QColor(QString::fromStdString(
               paletteSettings.alert_color(phenomenon, false).GetDefault())));
      }

      // Configure font dialog
      fontDialog_->setOptions(
         QFontDialog::FontDialogOption::DontUseNativeDialog |
         QFontDialog::FontDialogOption::ScalableFonts);
      fontDialog_->setWindowModality(Qt::WindowModality::WindowModal);
   }
   ~SettingsDialogImpl() = default;

   void ConnectSignals();
   void SetupGeneralTab();
   void SetupPalettesColorTablesTab();
   void SetupPalettesAlertsTab();
   void SetupPalettesRadarSiteStatusTab();
   void SetupUnitsTab();
   void SetupAudioTab();
   void SetupTextTab();
   void SetupHotkeysTab();

   void UpdateRadarDialogLocation(const std::string& id);
   void UpdateAlertRadarDialogLocation(const std::string& id);

   QFont GetSelectedFont();
   void  SelectFontCategory(types::FontCategory fontCategory);
   void  UpdateFontDisplayData();

   void ApplyChanges();
   void DiscardChanges();
   void ResetToDefault();

   static QImage
   GenerateColorTableImage(std::shared_ptr<common::ColorTable> colorTable,
                           std::uint16_t                       min,
                           std::uint16_t                       max,
                           float                               offset,
                           float                               scale);
   static void LoadColorTablePreview(const std::string& key,
                                     const std::string& value,
                                     QLabel*            imageLabel);
   static std::string
   RadarSiteLabel(std::shared_ptr<config::RadarSite>& radarSite);

   SettingsDialog*   self_;
   RadarSiteDialog*  radarSiteDialog_;
   RadarSiteDialog*  alertAudioRadarSiteDialog_;
   SerialPortDialog* gpsSourceDialog_;
   CountyDialog*     countyDialog_;
   WFODialog*        wfoDialog_;
   QFontDialog*      fontDialog_;

   QMapLibre::Settings& mapSettings_;

   QStandardItemModel* fontCategoryModel_;

   types::FontCategory selectedFontCategory_ {types::FontCategory::Unknown};

   std::shared_ptr<manager::MediaManager> mediaManager_ {
      manager::MediaManager::Instance()};
   std::shared_ptr<manager::PositionManager> positionManager_ {
      manager::PositionManager::Instance()};

   std::vector<SettingsPageWidget*> settingsPages_ {};
   AlertPaletteSettingsWidget*      alertPaletteSettingsWidget_ {};
   HotkeySettingsWidget*            hotkeySettingsWidget_ {};
   UnitSettingsWidget*              unitSettingsWidget_ {};

   RadarSiteStatusPaletteSettingsWidget*
      radarSiteStatusPaletteSettingsWidget_ {};

   settings::SettingsInterface<std::string>  defaultRadarSite_ {};
   settings::SettingsInterface<std::int64_t> gridWidth_ {};
   settings::SettingsInterface<std::int64_t> gridHeight_ {};
   settings::SettingsInterface<std::string>  mapProvider_ {};
   settings::SettingsInterface<std::string>  mapboxApiKey_ {};
   settings::SettingsInterface<std::string>  mapTilerApiKey_ {};
   settings::SettingsInterface<std::string>  defaultAlertAction_ {};
   settings::SettingsInterface<std::string>  clockFormat_ {};
   settings::SettingsInterface<std::string>  customStyleDrawLayer_ {};
   settings::SettingsInterface<std::string>  customStyleUrl_ {};
   settings::SettingsInterface<std::string>  defaultTimeZone_ {};
   settings::SettingsInterface<std::string>  positioningPlugin_ {};
   settings::SettingsInterface<std::int64_t> nmeaBaudRate_ {};
   settings::SettingsInterface<std::string>  nmeaSource_ {};
   settings::SettingsInterface<std::string>  screenCaptureFolder_ {};
   settings::SettingsInterface<std::string>  screenCaptureName_ {};
   settings::SettingsInterface<std::string>  theme_ {};
   settings::SettingsInterface<std::string>  themeFile_ {};
   settings::SettingsInterface<std::string>  warningsProvider_ {};
   settings::SettingsInterface<double>       radarSiteThreshold_ {};
   settings::SettingsInterface<bool>         antiAliasingEnabled_ {};
   settings::SettingsInterface<bool>         autoNavigateToWsr88dOnly_ {};
   settings::SettingsInterface<bool>         centerOnRadarSelection_ {};
   settings::SettingsInterface<bool>         screenCaptureOnRefresh_ {};
   settings::SettingsInterface<bool>         showMapAttribution_ {};
   settings::SettingsInterface<bool>         showMapCenter_ {};
   settings::SettingsInterface<bool>         showMapLogo_ {};
   settings::SettingsInterface<bool>         showSmoothedRangeFolding_ {};
   settings::SettingsInterface<bool>         updateNotificationsEnabled_ {};
   settings::SettingsInterface<bool>         cursorIconAlwaysOn_ {};
   settings::SettingsInterface<double>       cursorIconScale_ {};
   settings::SettingsInterface<bool>         debugEnabled_ {};

   std::unordered_map<std::string, settings::SettingsInterface<std::string>>
      colorTables_ {};

   settings::SettingsInterface<std::string> alertAudioSoundFile_ {};
   settings::SettingsInterface<std::string> alertAudioLocationMethod_ {};
   settings::SettingsInterface<double>      alertAudioLatitude_ {};
   settings::SettingsInterface<double>      alertAudioLongitude_ {};
   settings::SettingsInterface<std::string> alertAudioRadarSite_ {};
   settings::SettingsInterface<double>      alertAudioRadius_ {};
   settings::SettingsInterface<std::string> alertAudioCounty_ {};
   settings::SettingsInterface<std::string> alertAudioWFO_ {};

   std::unordered_map<awips::Phenomenon, settings::SettingsInterface<bool>>
      alertAudioEnabled_ {};

   settings::SettingsInterface<std::int64_t> masterVolume_ {};

   std::unordered_map<types::FontCategory,
                      settings::SettingsInterface<std::string>>
      fontFamilies_ {};
   std::unordered_map<types::FontCategory,
                      settings::SettingsInterface<std::string>>
      fontStyles_ {};
   std::unordered_map<types::FontCategory, settings::SettingsInterface<double>>
      fontPointSizes_ {};

   settings::SettingsInterface<std::int64_t> hoverTextWrap_ {};
   settings::SettingsInterface<std::string>  tooltipMethod_ {};
   settings::SettingsInterface<bool>         placefileTextDropShadowEnabled_ {};
   settings::SettingsInterface<bool>         radarSiteHoverTextEnabled_ {};

   std::vector<settings::SettingsInterfaceBase*> settings_;
};

SettingsDialog::SettingsDialog(QMapLibre::Settings& mapSettings,
                               QWidget*             parent) :
    QDialog(parent),
    p {std::make_unique<SettingsDialogImpl>(this, mapSettings)},
    ui(new Ui::SettingsDialog)
{
   ui->setupUi(this);

   // Set OK as default
   ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setDefault(true);

   // General
   p->SetupGeneralTab();

   // Palettes > Color Tables
   p->SetupPalettesColorTablesTab();

   // Palettes > Alerts
   p->SetupPalettesAlertsTab();

   // Palettes > Radar Site Status
   p->SetupPalettesRadarSiteStatusTab();

   // Units
   p->SetupUnitsTab();

   // Audio
   p->SetupAudioTab();

   // Text
   p->SetupTextTab();

   // Hotkeys
   p->SetupHotkeysTab();

   p->ConnectSignals();
}

SettingsDialog::~SettingsDialog()
{
   delete ui;
}

void SettingsDialog::RefreshWidgets()
{
   for (auto& setting : p->settings_)
   {
      setting->Reset();
   }
}

void SettingsDialogImpl::ConnectSignals()
{
   QObject::connect(self_->ui->listWidget,
                    &QListWidget::currentRowChanged,
                    self_->ui->stackedWidget,
                    &QStackedWidget::setCurrentIndex);

   QObject::connect(self_->ui->radarSiteSelectButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { radarSiteDialog_->show(); });

   QObject::connect(radarSiteDialog_,
                    &RadarSiteDialog::accepted,
                    self_,
                    [this]()
                    {
                       std::string id = radarSiteDialog_->radar_site();

                       std::shared_ptr<config::RadarSite> radarSite =
                          config::RadarSite::Get(id);

                       if (radarSite != nullptr)
                       {
                          self_->ui->radarSiteComboBox->setCurrentText(
                             QString::fromStdString(RadarSiteLabel(radarSite)));
                       }
                    });

   QObject::connect(self_->ui->alertAudioRadarSiteSelectButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { alertAudioRadarSiteDialog_->show(); });

   QObject::connect(
      alertAudioRadarSiteDialog_,
      &RadarSiteDialog::accepted,
      self_,
      [this]()
      {
         std::string id = alertAudioRadarSiteDialog_->radar_site();

         std::shared_ptr<config::RadarSite> radarSite =
            config::RadarSite::Get(id);

         if (radarSite != nullptr)
         {
            self_->ui->alertAudioRadarSiteComboBox->setCurrentText(
               QString::fromStdString(RadarSiteLabel(radarSite)));
         }
      });

   QObject::connect(self_->ui->gpsSourceSelectButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { gpsSourceDialog_->show(); });

   QObject::connect(gpsSourceDialog_,
                    &SerialPortDialog::accepted,
                    self_,
                    [this]()
                    {
                       std::string serialPort = gpsSourceDialog_->serial_port();
                       int         baudRate   = gpsSourceDialog_->baud_rate();

                       if (!serialPort.empty() && serialPort != "?")
                       {
                          std::string source =
                             fmt::format("serial:{}", serialPort);
                          nmeaSource_.StageValue(source);
                       }

                       if (baudRate > 0)
                       {
                          self_->ui->nmeaBaudRateSpinBox->setValue(baudRate);
                       }
                    });

   // Update the Radar Site dialog "map" location with the currently selected
   // radar site
   auto& defaultRadarSite = *defaultRadarSite_.GetSettingsVariable();
   defaultRadarSite.RegisterValueStagedCallback(
      [this](const std::string& newValue)
      { UpdateRadarDialogLocation(newValue); });

   QObject::connect(
      self_->ui->alertAudioSoundTestButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         mediaManager_->Play(
            self_->ui->alertAudioSoundLineEdit->text().toStdString());
      });
   QObject::connect(self_->ui->alertAudioSoundStopButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { mediaManager_->Stop(); });

   QObject::connect(
      self_->ui->fontListView->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      self_,
      [this](const QItemSelection& selected, const QItemSelection& deselected)
      {
         if (selected.size() == 0 && deselected.size() == 0)
         {
            // Items which stay selected but change their index are not
            // included in selected and deselected. Thus, this signal might
            // be emitted with both selected and deselected empty, if only
            // the indices of selected items change.
            return;
         }

         if (selected.size() > 0)
         {
            QModelIndex selectedIndex = selected[0].indexes()[0];
            QVariant    variantData =
               self_->ui->fontListView->model()->data(selectedIndex);
            if (variantData.typeId() == QMetaType::QString)
            {
               types::FontCategory fontCategory =
                  types::GetFontCategory(variantData.toString().toStdString());
               SelectFontCategory(fontCategory);
               UpdateFontDisplayData();
            }
         }
      });

   QObject::connect(self_->ui->fontSelectButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]()
                    {
                       fontDialog_->setCurrentFont(GetSelectedFont());
                       fontDialog_->show();
                    });

   QObject::connect(fontDialog_,
                    &QFontDialog::fontSelected,
                    self_,
                    [this](const QFont& font)
                    {
                       fontFamilies_.at(selectedFontCategory_)
                          .StageValue(font.family().toStdString());
                       fontStyles_.at(selectedFontCategory_)
                          .StageValue(font.styleName().toStdString());
                       fontPointSizes_.at(selectedFontCategory_)
                          .StageValue(font.pointSizeF());

                       UpdateFontDisplayData();
                    });

   QObject::connect(self_->ui->resetFontButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]()
                    {
                       fontFamilies_.at(selectedFontCategory_).StageDefault();
                       fontStyles_.at(selectedFontCategory_).StageDefault();
                       fontPointSizes_.at(selectedFontCategory_).StageDefault();

                       UpdateFontDisplayData();
                    });

   QObject::connect(
      self_->ui->buttonBox,
      &QDialogButtonBox::clicked,
      self_,
      [this](QAbstractButton* button)
      {
         QDialogButtonBox::ButtonRole role =
            self_->ui->buttonBox->buttonRole(button);

         switch (role)
         {
         case QDialogButtonBox::ButtonRole::AcceptRole: // OK
         case QDialogButtonBox::ButtonRole::ApplyRole:  // Apply
            ApplyChanges();
            break;

         case QDialogButtonBox::ButtonRole::DestructiveRole: // Discard
         case QDialogButtonBox::ButtonRole::RejectRole:      // Cancel
            DiscardChanges();
            break;

         case QDialogButtonBox::ButtonRole::ResetRole: // Restore Defaults
            ResetToDefault();
            break;

         default:
            break;
         }
      });
}

void SettingsDialogImpl::SetupGeneralTab()
{
   settings::GeneralSettings& generalSettings =
      settings::GeneralSettings::Instance();
   settings::ProductSettings& productSettings =
      settings::ProductSettings::Instance();

   QObject::connect(
      self_->ui->themeComboBox,
      &QComboBox::currentTextChanged,
      self_,
      [this](const QString& text)
      {
         const types::UiStyle style  = types::GetUiStyle(text.toStdString());
         const bool themeFileEnabled = style == types::UiStyle::FusionCustom;

         self_->ui->themeFileLineEdit->setEnabled(themeFileEnabled);
         self_->ui->themeFileSelectButton->setEnabled(themeFileEnabled);
         self_->ui->resetThemeFileButton->setEnabled(themeFileEnabled);
      });

   theme_.SetSettingsVariable(generalSettings.theme());
   SCWX_SETTINGS_COMBO_BOX(theme_,
                           self_->ui->themeComboBox,
                           types::UiStyleIterator(),
                           types::GetUiStyleName);
   theme_.SetResetButton(self_->ui->resetThemeButton);

   themeFile_.SetSettingsVariable(generalSettings.theme_file());
   themeFile_.SetEditWidget(self_->ui->themeFileLineEdit);
   themeFile_.SetResetButton(self_->ui->resetThemeFileButton);
   themeFile_.EnableTrimming();

   QObject::connect(
      self_->ui->themeFileSelectButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         const settings::GeneralSettings& generalSettings =
            settings::GeneralSettings::Instance();
         QString file = generalSettings.theme_file().GetStagedOrValue().c_str();

         if (file.isEmpty())
         {
            const std::filesystem::path settingsPath {
               main::ApplicationPaths::GetLocation(
                  main::ApplicationPaths::StandardLocation::Settings)};
            const std::filesystem::path defaultFilePath =
               settingsPath / "theme.conf";
            file = QString::fromStdString(defaultFilePath.generic_string());
            self_->ui->themeFileLineEdit->setText(file);
            // setText does not emit the textEdited signal
            Q_EMIT self_->ui->themeFileLineEdit->textEdited(file);
         }

         const QPalette palette =
            Qt6CT::loadColorScheme(file, QApplication::palette());
         QStyle* style = QApplication::style();

         // WA_DeleteOnClose manages memory
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         auto* dialog = new PaletteEditDialog(palette, style, self_);
         dialog->setAttribute(Qt::WA_DeleteOnClose);

         QObject::connect(
            dialog,
            &QDialog::accepted,
            self_,
            [dialog]()
            {
               const QPalette palette = dialog->selectedPalette();
               const settings::GeneralSettings& generalSettings =
                  settings::GeneralSettings::Instance();
               const QString file =
                  generalSettings.theme_file().GetStagedOrValue().c_str();
               Qt6CT::createColorScheme(file, palette);

               auto uiStyle = scwx::qt::types::GetUiStyle(
                  generalSettings.theme().GetValue());
               if (uiStyle == scwx::qt::types::UiStyle::FusionCustom)
               {
                  QApplication::setPalette(palette);
               }
            });
         dialog->open();
      });

   auto radarSites = config::RadarSite::GetAll();

   // Sort radar sites by ID
   std::sort(radarSites.begin(),
             radarSites.end(),
             [](const std::shared_ptr<config::RadarSite>& a,
                const std::shared_ptr<config::RadarSite>& b)
             { return a->id() < b->id(); });

   // Add default and follow options to radar sites
   self_->ui->alertAudioRadarSiteComboBox->addItem(
      QString::fromStdString("default"));
   self_->ui->alertAudioRadarSiteComboBox->addItem(
      QString::fromStdString("follow"));

   // Add sorted radar sites
   for (std::shared_ptr<config::RadarSite>& radarSite : radarSites)
   {
      QString text = QString::fromStdString(RadarSiteLabel(radarSite));
      self_->ui->radarSiteComboBox->addItem(text);
      self_->ui->alertAudioRadarSiteComboBox->addItem(text);
   }

   defaultRadarSite_.SetSettingsVariable(generalSettings.default_radar_site());
   defaultRadarSite_.SetMapFromValueFunction(
      [](const std::string& id) -> std::string
      {
         // Get the radar site associated with the ID
         std::shared_ptr<config::RadarSite> radarSite =
            config::RadarSite::Get(id);

         if (radarSite == nullptr)
         {
            // No radar site found, just return the ID
            return id;
         }

         // Add location details to the radar site
         return RadarSiteLabel(radarSite);
      });
   defaultRadarSite_.SetMapToValueFunction(
      [](const std::string& text) -> std::string
      {
         // Find the position of location details
         size_t pos = text.find(" (");

         if (pos == std::string::npos)
         {
            // No location details found, just return the text
            return text;
         }

         // Remove location details from the radar site
         return text.substr(0, pos);
      });
   defaultRadarSite_.SetEditWidget(self_->ui->radarSiteComboBox);
   defaultRadarSite_.SetResetButton(self_->ui->resetRadarSiteButton);
   UpdateRadarDialogLocation(generalSettings.default_radar_site().GetValue());

   gridWidth_.SetSettingsVariable(generalSettings.grid_width());
   gridWidth_.SetEditWidget(self_->ui->gridWidthSpinBox);
   gridWidth_.SetResetButton(self_->ui->resetGridWidthButton);

   gridHeight_.SetSettingsVariable(generalSettings.grid_height());
   gridHeight_.SetEditWidget(self_->ui->gridHeightSpinBox);
   gridHeight_.SetResetButton(self_->ui->resetGridHeightButton);

   mapProvider_.SetSettingsVariable(generalSettings.map_provider());
   SCWX_SETTINGS_COMBO_BOX(mapProvider_,
                           self_->ui->mapProviderComboBox,
                           map::MapProviderIterator(),
                           map::GetMapProviderName);
   mapProvider_.SetResetButton(self_->ui->resetMapProviderButton);

   self_->ui->mapboxApiKeyLineEdit->setMapProvider(map::MapProvider::Mapbox);
   mapboxApiKey_.SetSettingsVariable(generalSettings.mapbox_api_key());
   mapboxApiKey_.SetEditWidget(self_->ui->mapboxApiKeyLineEdit);
   mapboxApiKey_.SetResetButton(self_->ui->resetMapboxApiKeyButton);
   mapboxApiKey_.EnableTrimming();

   self_->ui->mapTilerApiKeyLineEdit->setMapProvider(
      map::MapProvider::MapTiler);
   mapTilerApiKey_.SetSettingsVariable(generalSettings.maptiler_api_key());
   mapTilerApiKey_.SetEditWidget(self_->ui->mapTilerApiKeyLineEdit);
   mapTilerApiKey_.SetResetButton(self_->ui->resetMapTilerApiKeyButton);
   mapTilerApiKey_.EnableTrimming();

   QObject::connect(self_->ui->mapProviderComboBox,
                    &QComboBox::currentTextChanged,
                    self_,
                    [this](const QString& text)
                    {
                       const map::MapProvider mapProvider =
                          map::GetMapProvider(text.toStdString());
                       const bool providerHasLocalFiles =
                          mapProvider == map::MapProvider::OpenFreeMap;
                       self_->ui->customMapUrlToolButton->setEnabled(
                          providerHasLocalFiles);
                    });

   customStyleUrl_.SetSettingsVariable(generalSettings.custom_style_url());
   customStyleUrl_.SetEditWidget(self_->ui->customMapUrlLineEdit);
   customStyleUrl_.SetResetButton(self_->ui->resetCustomMapUrlButton);
   customStyleUrl_.SetInvalidTooltip(
      "Remove anything following \"?key=\" in the URL");
   customStyleUrl_.EnableTrimming();
   QObject::connect(
      self_->ui->customMapUrlToolButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         static const std::string styleFilter = "Map Style (*.json)";
         static const std::string allFilter   = "All Files (*)";

         // WA_DeleteOnClose manages memory
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         auto dialog = new QFileDialog(self_);
         dialog->setAttribute(Qt::WA_DeleteOnClose);
         dialog->setFileMode(QFileDialog::ExistingFile);
         dialog->setNameFilters(
            {QObject::tr(styleFilter.c_str()), QObject::tr(allFilter.c_str())});

         QObject::connect(dialog,
                          &QFileDialog::fileSelected,
                          self_,
                          [this](const QString& file)
                          {
                             const QString path =
                                QDir::toNativeSeparators(file);

                             logger_->info("Selected Custom Style URL file: {}",
                                           path.toStdString());

                             self_->ui->customMapUrlLineEdit->setText(path);
                             // setText does not emit the textEdited signal
                             self_->ui->customMapUrlLineEdit->textEdited(path);
                          });
         dialog->open();
      });

   customStyleDrawLayer_.SetSettingsVariable(
      generalSettings.custom_style_draw_layer());
   customStyleDrawLayer_.SetEditWidget(self_->ui->customMapLayerLineEdit);
   customStyleDrawLayer_.SetResetButton(self_->ui->resetCustomMapLayerButton);
   QObject::connect(
      self_->ui->customMapLayerToolButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         // WA_DeleteOnClose manages memory
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         auto* customLayerDialog = new ui::CustomLayerDialog(mapSettings_);
         customLayerDialog->setAttribute(Qt::WA_DeleteOnClose);
         QObject::connect(
            customLayerDialog,
            &QDialog::accepted,
            self_,
            [this, customLayerDialog]()
            {
               auto newLayer = customLayerDialog->selected_layer();
               self_->ui->customMapLayerLineEdit->setText(newLayer.c_str());
               // setText does not emit the textEdited signal
               Q_EMIT
               self_->ui->customMapLayerLineEdit->textEdited(newLayer.c_str());
            });

         customLayerDialog->open();
      });

   screenCaptureFolder_.SetSettingsVariable(
      generalSettings.screen_capture_folder());
   screenCaptureFolder_.SetEditWidget(self_->ui->screenCaptureFolderLineEdit);
   screenCaptureFolder_.SetResetButton(
      self_->ui->resetScreenCaptureFolderButton);
   QObject::connect(
      self_->ui->screenCaptureFolderButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         // WA_DeleteOnClose manages memory
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
         auto dialog = new QFileDialog(self_);
         dialog->setAttribute(Qt::WA_DeleteOnClose);
         dialog->setFileMode(QFileDialog::FileMode::Directory);

         QObject::connect(
            dialog,
            &QFileDialog::fileSelected,
            self_,
            [this](const QString& file)
            {
               const QString path = QDir::toNativeSeparators(file);

               logger_->info("Selected screen capture folder: {}",
                             path.toStdString());
               self_->ui->screenCaptureFolderLineEdit->setText(path);

               // setText does not emit the textEdited signal
               Q_EMIT self_->ui->screenCaptureFolderLineEdit->textEdited(path);
            });

         dialog->open();
      });

   screenCaptureName_.SetSettingsVariable(
      generalSettings.screen_capture_name());
   screenCaptureName_.SetEditWidget(self_->ui->screenCaptureNameLineEdit);
   screenCaptureName_.SetResetButton(self_->ui->resetScreenCaptureNameButton);

   defaultAlertAction_.SetSettingsVariable(
      generalSettings.default_alert_action());
   SCWX_SETTINGS_COMBO_BOX(defaultAlertAction_,
                           self_->ui->defaultAlertActionComboBox,
                           types::AlertActionIterator(),
                           types::GetAlertActionName);
   defaultAlertAction_.SetResetButton(self_->ui->resetDefaultAlertActionButton);

   clockFormat_.SetSettingsVariable(generalSettings.clock_format());
   SCWX_SETTINGS_COMBO_BOX(clockFormat_,
                           self_->ui->clockFormatComboBox,
                           scwx::util::ClockFormatIterator(),
                           scwx::util::GetClockFormatName);
   clockFormat_.SetResetButton(self_->ui->resetClockFormatButton);

   defaultTimeZone_.SetSettingsVariable(generalSettings.default_time_zone());
   SCWX_SETTINGS_COMBO_BOX(defaultTimeZone_,
                           self_->ui->defaultTimeZoneComboBox,
                           types::DefaultTimeZoneIterator(),
                           types::GetDefaultTimeZoneName);
   defaultTimeZone_.SetResetButton(self_->ui->resetDefaultTimeZoneButton);

   QObject::connect(
      self_->ui->positioningPluginComboBox,
      &QComboBox::currentTextChanged,
      self_,
      [this](const QString& text)
      {
         types::PositioningPlugin positioningPlugin =
            types::GetPositioningPlugin(text.toStdString());

         bool gpsSourceEnabled =
            positioningPlugin == types::PositioningPlugin::Nmea;

         self_->ui->nmeaSourceLineEdit->setEnabled(gpsSourceEnabled);
         self_->ui->gpsSourceSelectButton->setEnabled(gpsSourceEnabled);
         self_->ui->nmeaBaudRateSpinBox->setEnabled(gpsSourceEnabled);
         self_->ui->resetNmeaSourceButton->setEnabled(gpsSourceEnabled);
         self_->ui->resetNmeaBaudRateButton->setEnabled(gpsSourceEnabled);
      });

   positioningPlugin_.SetSettingsVariable(generalSettings.positioning_plugin());
   SCWX_SETTINGS_COMBO_BOX(positioningPlugin_,
                           self_->ui->positioningPluginComboBox,
                           types::PositioningPluginIterator(),
                           types::GetPositioningPluginName);
   positioningPlugin_.SetResetButton(self_->ui->resetPositioningPluginButton);

   nmeaBaudRate_.SetSettingsVariable(generalSettings.nmea_baud_rate());
   nmeaBaudRate_.SetEditWidget(self_->ui->nmeaBaudRateSpinBox);
   nmeaBaudRate_.SetResetButton(self_->ui->resetNmeaBaudRateButton);

   nmeaSource_.SetSettingsVariable(generalSettings.nmea_source());
   nmeaSource_.SetEditWidget(self_->ui->nmeaSourceLineEdit);
   nmeaSource_.SetResetButton(self_->ui->resetNmeaSourceButton);
   nmeaSource_.EnableTrimming();

   warningsProvider_.SetSettingsVariable(generalSettings.warnings_provider());
   warningsProvider_.SetEditWidget(self_->ui->warningsProviderLineEdit);
   warningsProvider_.SetResetButton(self_->ui->resetWarningsProviderButton);
   warningsProvider_.EnableTrimming();

   radarSiteThreshold_.SetSettingsVariable(
      generalSettings.radar_site_threshold());
   radarSiteThreshold_.SetEditWidget(self_->ui->radarSiteThresholdSpinBox);
   radarSiteThreshold_.SetResetButton(self_->ui->resetRadarSiteThresholdButton);
   radarSiteThreshold_.SetUnitLabel(self_->ui->radarSiteThresholdUnitLabel);
   auto radarSiteThresholdUpdateUnits = [this](const std::string& newValue)
   {
      const types::DistanceUnits radiusUnits =
         types::GetDistanceUnitsFromName(newValue);
      const double      radiusScale = types::GetDistanceUnitsScale(radiusUnits);
      const std::string abbreviation =
         types::GetDistanceUnitsAbbreviation(radiusUnits);

      radarSiteThreshold_.SetUnit(radiusScale, abbreviation);
   };
   settings::UnitSettings::Instance()
      .distance_units()
      .RegisterValueStagedCallback(radarSiteThresholdUpdateUnits);
   radarSiteThresholdUpdateUnits(
      settings::UnitSettings::Instance().distance_units().GetValue());

   cursorIconScale_.SetSettingsVariable(generalSettings.cursor_icon_scale());
   cursorIconScale_.SetEditWidget(self_->ui->cursorIconScaleSpinBox);
   cursorIconScale_.SetResetButton(self_->ui->resetCursorIconScaleButton);

   antiAliasingEnabled_.SetSettingsVariable(
      generalSettings.anti_aliasing_enabled());
   antiAliasingEnabled_.SetEditWidget(self_->ui->antiAliasingEnabledCheckBox);

   autoNavigateToWsr88dOnly_.SetSettingsVariable(
      generalSettings.auto_navigate_to_wsr88d_only());
   autoNavigateToWsr88dOnly_.SetEditWidget(
      self_->ui->autoNavigateToWsr88dOnlyCheckBox);

   centerOnRadarSelection_.SetSettingsVariable(
      generalSettings.center_on_radar_selection());
   centerOnRadarSelection_.SetEditWidget(
      self_->ui->centerOnRadarSelectionCheckBox);

   screenCaptureOnRefresh_.SetSettingsVariable(
      generalSettings.screen_capture_on_refresh());
   screenCaptureOnRefresh_.SetEditWidget(
      self_->ui->screenCaptureOnRefreshCheckBox);

   showMapAttribution_.SetSettingsVariable(
      generalSettings.show_map_attribution());
   showMapAttribution_.SetEditWidget(self_->ui->showMapAttributionCheckBox);

   showMapCenter_.SetSettingsVariable(generalSettings.show_map_center());
   showMapCenter_.SetEditWidget(self_->ui->showMapCenterCheckBox);

   showMapLogo_.SetSettingsVariable(generalSettings.show_map_logo());
   showMapLogo_.SetEditWidget(self_->ui->showMapLogoCheckBox);

   showSmoothedRangeFolding_.SetSettingsVariable(
      productSettings.show_smoothed_range_folding());
   showSmoothedRangeFolding_.SetEditWidget(
      self_->ui->showSmoothedRangeFoldingCheckBox);

   updateNotificationsEnabled_.SetSettingsVariable(
      generalSettings.update_notifications_enabled());
   updateNotificationsEnabled_.SetEditWidget(
      self_->ui->enableUpdateNotificationsCheckBox);

   cursorIconAlwaysOn_.SetSettingsVariable(
      generalSettings.cursor_icon_always_on());
   cursorIconAlwaysOn_.SetEditWidget(self_->ui->cursorIconAlwaysOnCheckBox);

   debugEnabled_.SetSettingsVariable(generalSettings.debug_enabled());
   debugEnabled_.SetEditWidget(self_->ui->debugEnabledCheckBox);
}

void SettingsDialogImpl::SetupPalettesColorTablesTab()
{
   settings::PaletteSettings& paletteSettings =
      settings::PaletteSettings::Instance();

   // Palettes > Color Tables
   QGridLayout* colorTableLayout =
      reinterpret_cast<QGridLayout*>(self_->ui->colorTableContents->layout());

   colorTables_.reserve(kColorTableTypes_.size());

   int colorTableRow = 0;
   for (auto& colorTableType : kColorTableTypes_)
   {
      QLabel*      imageLabel     = new QLabel(self_);
      QLineEdit*   lineEdit       = new QLineEdit(self_);
      QToolButton* openFileButton = new QToolButton(self_);
      QToolButton* resetButton    = new QToolButton(self_);

      openFileButton->setText(QObject::tr("..."));

      resetButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
      resetButton->setVisible(false);

      imageLabel->setFrameShape(QFrame::Shape::Box);
      imageLabel->setFrameShadow(QFrame::Shadow::Plain);
      imageLabel->setVisible(false);

      colorTableLayout->addWidget(
         new QLabel(colorTableType.second.c_str(), self_), colorTableRow, 0);
      colorTableLayout->addWidget(imageLabel, colorTableRow, 1);
      colorTableLayout->addWidget(lineEdit, colorTableRow, 2);
      colorTableLayout->addWidget(openFileButton, colorTableRow, 3);
      colorTableLayout->addWidget(resetButton, colorTableRow, 4);
      ++colorTableRow;

      // Create settings interface
      auto result = colorTables_.emplace(
         colorTableType.first, settings::SettingsInterface<std::string> {});
      auto& pair       = *result.first;
      auto& colorTable = pair.second;

      // Add to settings list
      settings_.push_back(&colorTable);

      auto& colorTableVariable = paletteSettings.palette(colorTableType.first);
      colorTable.SetSettingsVariable(colorTableVariable);
      colorTable.SetEditWidget(lineEdit);
      colorTable.SetResetButton(resetButton);
      colorTable.EnableTrimming();

      colorTableVariable.RegisterValueStagedCallback(
         [colorTableType, imageLabel](const std::string& value)
         { LoadColorTablePreview(colorTableType.first, value, imageLabel); });

      LoadColorTablePreview(
         colorTableType.first, colorTableVariable.GetValue(), imageLabel);

      QObject::connect(
         openFileButton,
         &QAbstractButton::clicked,
         self_,
         [this, lineEdit]()
         {
            static const std::string paletteFilter = "Color Palettes (*.pal)";
            static const std::string allFilter     = "All Files (*)";

            QFileDialog* dialog = new QFileDialog(self_);

            dialog->setFileMode(QFileDialog::ExistingFile);
            dialog->setNameFilters({QObject::tr(paletteFilter.c_str()),
                                    QObject::tr(allFilter.c_str())});
            dialog->setAttribute(Qt::WA_DeleteOnClose);

            QObject::connect(dialog,
                             &QFileDialog::fileSelected,
                             self_,
                             [lineEdit](const QString& file)
                             {
                                QString path = QDir::toNativeSeparators(file);

                                logger_->info("Selected palette: {}",
                                              path.toStdString());
                                lineEdit->setText(path);

                                // setText does not emit the textEdited signal
                                Q_EMIT lineEdit->textEdited(path);
                             });

            dialog->open();
         });
   }
}

void SettingsDialogImpl::SetupPalettesAlertsTab()
{
   // Palettes > Alerts
   QVBoxLayout* layout = new QVBoxLayout(self_->ui->alertsPalette);

   alertPaletteSettingsWidget_ =
      new AlertPaletteSettingsWidget(self_->ui->alertsPalette);
   layout->addWidget(alertPaletteSettingsWidget_);

   settingsPages_.push_back(alertPaletteSettingsWidget_);
}

void SettingsDialogImpl::SetupPalettesRadarSiteStatusTab()
{
   // Palettes > Radar Site Status
   auto layout = new QVBoxLayout(self_->ui->radarSiteStatusPalette);

   radarSiteStatusPaletteSettingsWidget_ =
      new RadarSiteStatusPaletteSettingsWidget(
         self_->ui->radarSiteStatusPalette);
   layout->addWidget(radarSiteStatusPaletteSettingsWidget_);

   settingsPages_.push_back(radarSiteStatusPaletteSettingsWidget_);
}

void SettingsDialogImpl::SetupUnitsTab()
{
   QVBoxLayout* layout = new QVBoxLayout(self_->ui->units);

   unitSettingsWidget_ = new UnitSettingsWidget(self_->ui->units);
   layout->addWidget(unitSettingsWidget_);

   settingsPages_.push_back(unitSettingsWidget_);
}

void SettingsDialogImpl::SetupAudioTab()
{
   QObject::connect(
      self_->ui->alertAudioLocationMethodComboBox,
      &QComboBox::currentTextChanged,
      self_,
      [this](const QString& text)
      {
         types::LocationMethod locationMethod =
            types::GetLocationMethod(text.toStdString());

         bool coordinateEntryEnabled =
            locationMethod == types::LocationMethod::Fixed;
         bool radarSiteEntryEnable =
            locationMethod == types::LocationMethod::RadarSite;
         bool radiusEntryEnable =
            locationMethod == types::LocationMethod::Fixed ||
            locationMethod == types::LocationMethod::Track ||
            locationMethod == types::LocationMethod::RadarSite;
         bool countyEntryEnabled =
            locationMethod == types::LocationMethod::County;
         bool wfoEntryEnabled = locationMethod == types::LocationMethod::WFO;

         self_->ui->alertAudioLatitudeSpinBox->setEnabled(
            coordinateEntryEnabled);
         self_->ui->alertAudioLongitudeSpinBox->setEnabled(
            coordinateEntryEnabled);
         self_->ui->resetAlertAudioLatitudeButton->setEnabled(
            coordinateEntryEnabled);
         self_->ui->resetAlertAudioLongitudeButton->setEnabled(
            coordinateEntryEnabled);

         self_->ui->alertAudioRadarSiteComboBox->setEnabled(
            radarSiteEntryEnable);
         self_->ui->alertAudioRadarSiteSelectButton->setEnabled(
            radarSiteEntryEnable);
         self_->ui->resetAlertAudioRadarSiteButton->setEnabled(
            radarSiteEntryEnable);

         self_->ui->alertAudioRadiusSpinBox->setEnabled(radiusEntryEnable);
         self_->ui->resetAlertAudioRadiusButton->setEnabled(radiusEntryEnable);

         self_->ui->alertAudioCountyLineEdit->setEnabled(countyEntryEnabled);
         self_->ui->alertAudioCountySelectButton->setEnabled(
            countyEntryEnabled);
         self_->ui->resetAlertAudioCountyButton->setEnabled(countyEntryEnabled);

         self_->ui->alertAudioWFOLineEdit->setEnabled(wfoEntryEnabled);
         self_->ui->alertAudioWFOSelectButton->setEnabled(wfoEntryEnabled);
         self_->ui->resetAlertAudioWFOButton->setEnabled(wfoEntryEnabled);
      });

   settings::AudioSettings& audioSettings = settings::AudioSettings::Instance();

   alertAudioSoundFile_.SetSettingsVariable(audioSettings.alert_sound_file());
   alertAudioSoundFile_.SetEditWidget(self_->ui->alertAudioSoundLineEdit);
   alertAudioSoundFile_.SetResetButton(self_->ui->resetAlertAudioSoundButton);
   alertAudioSoundFile_.EnableTrimming();

   QObject::connect(
      self_->ui->alertAudioSoundSelectButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         static const std::string audioFilter =
            "Audio Files (*.3ga *.669 *.a52 *.aac *.ac3 *.adt *.adts *.aif "
            "*.aifc *.aiff *.amb *.amr *.aob *.ape *.au *.awb *.caf *.dts "
            "*.flac *.it *.kar *.m4a *.m4b *.m4p *.m5p *.mid *.mka *.mlp *.mod "
            "*.mpa *.mp1 *.mp2 *.mp3 *.mpc *.mpga *.mus *.oga *.ogg *.oma "
            "*.opus *.qcp *.ra *.rmi *.s3m *.sid *.spx *.tak *.thd *.tta *.voc "
            "*.vqf *.w64 *.wav *.wma *.wv *.xa *.xm)";
         static const std::string allFilter = "All Files (*)";

         QFileDialog* dialog = new QFileDialog(self_);

         dialog->setFileMode(QFileDialog::ExistingFile);
         dialog->setNameFilters(
            {QObject::tr(audioFilter.c_str()), QObject::tr(allFilter.c_str())});
         dialog->setAttribute(Qt::WA_DeleteOnClose);

         QObject::connect(
            dialog,
            &QFileDialog::fileSelected,
            self_,
            [this](const QString& file)
            {
               QString path = QDir::toNativeSeparators(file);

               logger_->info("Selected alert sound file: {}",
                             path.toStdString());
               self_->ui->alertAudioSoundLineEdit->setText(path);

               // setText does not emit the textEdited signal
               Q_EMIT self_->ui->alertAudioSoundLineEdit->textEdited(path);
            });

         dialog->open();
      });

   alertAudioLocationMethod_.SetSettingsVariable(
      audioSettings.alert_location_method());
   SCWX_SETTINGS_COMBO_BOX(alertAudioLocationMethod_,
                           self_->ui->alertAudioLocationMethodComboBox,
                           types::LocationMethodIterator(),
                           types::GetLocationMethodName);
   alertAudioLocationMethod_.SetResetButton(
      self_->ui->resetAlertAudioLocationMethodButton);

   alertAudioLatitude_.SetSettingsVariable(audioSettings.alert_latitude());
   alertAudioLatitude_.SetEditWidget(self_->ui->alertAudioLatitudeSpinBox);
   alertAudioLatitude_.SetResetButton(self_->ui->resetAlertAudioLatitudeButton);

   alertAudioLongitude_.SetSettingsVariable(audioSettings.alert_longitude());
   alertAudioLongitude_.SetEditWidget(self_->ui->alertAudioLongitudeSpinBox);
   alertAudioLongitude_.SetResetButton(
      self_->ui->resetAlertAudioLongitudeButton);

   alertAudioRadarSite_.SetSettingsVariable(audioSettings.alert_radar_site());
   alertAudioRadarSite_.SetMapFromValueFunction(
      [](const std::string& id) -> std::string
      {
         // Get the radar site associated with the ID
         std::shared_ptr<config::RadarSite> radarSite =
            config::RadarSite::Get(id);

         if (radarSite == nullptr)
         {
            // No radar site found, just return the ID
            return id;
         }

         // Add location details to the radar site
         return RadarSiteLabel(radarSite);
      });
   alertAudioRadarSite_.SetMapToValueFunction(
      [](const std::string& text) -> std::string
      {
         // Find the position of location details
         size_t pos = text.find(" (");

         if (pos == std::string::npos)
         {
            // No location details found, just return the text
            return text;
         }

         // Remove location details from the radar site
         return text.substr(0, pos);
      });
   alertAudioRadarSite_.SetEditWidget(self_->ui->alertAudioRadarSiteComboBox);
   alertAudioRadarSite_.SetResetButton(
      self_->ui->resetAlertAudioRadarSiteButton);
   UpdateAlertRadarDialogLocation(audioSettings.alert_radar_site().GetValue());

   alertAudioRadius_.SetSettingsVariable(audioSettings.alert_radius());
   alertAudioRadius_.SetEditWidget(self_->ui->alertAudioRadiusSpinBox);
   alertAudioRadius_.SetResetButton(self_->ui->resetAlertAudioRadiusButton);
   alertAudioRadius_.SetUnitLabel(self_->ui->alertAudioRadiusUnitsLabel);
   auto alertAudioRadiusUpdateUnits = [this](const std::string& newValue)
   {
      const types::DistanceUnits radiusUnits =
         types::GetDistanceUnitsFromName(newValue);
      const double      radiusScale = types::GetDistanceUnitsScale(radiusUnits);
      const std::string abbreviation =
         types::GetDistanceUnitsAbbreviation(radiusUnits);

      alertAudioRadius_.SetUnit(radiusScale, abbreviation);
   };
   settings::UnitSettings::Instance()
      .distance_units()
      .RegisterValueStagedCallback(alertAudioRadiusUpdateUnits);
   alertAudioRadiusUpdateUnits(
      settings::UnitSettings::Instance().distance_units().GetValue());

   auto& alertAudioPhenomena = types::GetAlertAudioPhenomena();
   auto  alertAudioLayout =
      static_cast<QGridLayout*>(self_->ui->alertAudioGroupBox->layout());

   alertAudioEnabled_.reserve(alertAudioPhenomena.size());

   for (const auto& phenomenon : alertAudioPhenomena)
   {
      QCheckBox* alertAudioCheckbox = new QCheckBox(self_);
      alertAudioCheckbox->setText(
         QString::fromStdString(awips::GetPhenomenonText(phenomenon)));

      static_cast<QGridLayout*>(self_->ui->alertAudioGroupBox->layout())
         ->addWidget(
            alertAudioCheckbox, alertAudioLayout->rowCount(), 0, 1, -1);

      // Create settings interface
      auto result = alertAudioEnabled_.emplace(
         phenomenon, settings::SettingsInterface<bool> {});
      auto& alertAudioEnabled = result.first->second;

      // Add to settings list
      settings_.push_back(&alertAudioEnabled);

      alertAudioEnabled.SetSettingsVariable(
         audioSettings.alert_enabled(phenomenon));
      alertAudioEnabled.SetEditWidget(alertAudioCheckbox);
   }

   QObject::connect(
      positionManager_.get(),
      &manager::PositionManager::PositionUpdated,
      self_,
      [this](const QGeoPositionInfo& info)
      {
         settings::AudioSettings& audioSettings =
            settings::AudioSettings::Instance();

         if (info.isValid() &&
             types::GetLocationMethod(
                audioSettings.alert_location_method().GetValue()) ==
                types::LocationMethod::Track)
         {
            QGeoCoordinate coordinate = info.coordinate();
            self_->ui->alertAudioLatitudeSpinBox->setValue(
               coordinate.latitude());
            self_->ui->alertAudioLongitudeSpinBox->setValue(
               coordinate.longitude());
         }
      });

   QObject::connect(
      self_->ui->alertAudioCountySelectButton,
      &QAbstractButton::clicked,
      self_,
      [this]()
      {
         std::string countyId =
            self_->ui->alertAudioCountyLineEdit->text().toStdString();

         if (countyId.length() >= 2)
         {
            countyDialog_->SelectState(countyId.substr(0, 2));
         }

         countyDialog_->show();
      });
   QObject::connect(countyDialog_,
                    &CountyDialog::accepted,
                    self_,
                    [this]()
                    {
                       std::string countyId  = countyDialog_->county_fips_id();
                       QString     qCountyId = QString::fromStdString(countyId);
                       self_->ui->alertAudioCountyLineEdit->setText(qCountyId);

                       // setText does not emit the textEdited signal
                       Q_EMIT self_->ui->alertAudioCountyLineEdit->textEdited(
                          qCountyId);
                    });
   QObject::connect(self_->ui->alertAudioCountyLineEdit,
                    &QLineEdit::textChanged,
                    self_,
                    [this](const QString& text)
                    {
                       std::string countyName =
                          config::CountyDatabase::GetCountyName(
                             text.toStdString());
                       self_->ui->alertAudioCountyLabel->setText(
                          QString::fromStdString(countyName));
                    });

   alertAudioCounty_.SetSettingsVariable(audioSettings.alert_county());
   alertAudioCounty_.SetEditWidget(self_->ui->alertAudioCountyLineEdit);
   alertAudioCounty_.SetResetButton(self_->ui->resetAlertAudioCountyButton);
   alertAudioCounty_.EnableTrimming();

   QObject::connect(self_->ui->alertAudioWFOSelectButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { wfoDialog_->show(); });
   QObject::connect(wfoDialog_,
                    &WFODialog::accepted,
                    self_,
                    [this]()
                    {
                       std::string wfoId  = wfoDialog_->wfo_id();
                       QString     qWFOId = QString::fromStdString(wfoId);
                       self_->ui->alertAudioWFOLineEdit->setText(qWFOId);

                       // setText does not emit the textEdited signal
                       Q_EMIT self_->ui->alertAudioWFOLineEdit->textEdited(
                          qWFOId);
                    });
   QObject::connect(self_->ui->alertAudioWFOLineEdit,
                    &QLineEdit::textChanged,
                    self_,
                    [this](const QString& text)
                    {
                       std::string wfoName = config::CountyDatabase::GetWFOName(
                          text.toStdString());
                       self_->ui->alertAudioWFOLabel->setText(
                          QString::fromStdString(wfoName));
                    });

   alertAudioWFO_.SetSettingsVariable(audioSettings.alert_wfo());
   alertAudioWFO_.SetEditWidget(self_->ui->alertAudioWFOLineEdit);
   alertAudioWFO_.SetResetButton(self_->ui->resetAlertAudioWFOButton);
   alertAudioWFO_.EnableTrimming();

   masterVolume_.SetSettingsVariable(audioSettings.master_volume());
   masterVolume_.SetEditWidget(self_->ui->masterVolumeSlider);
   masterVolume_.SetLabelWidget(self_->ui->masterVolumeLabel);
   masterVolume_.SetResetButton(self_->ui->resetMasterVolumeButton);
}

void SettingsDialogImpl::SetupTextTab()
{
   settings::TextSettings& textSettings = settings::TextSettings::Instance();

   self_->ui->fontListView->setModel(fontCategoryModel_);

   const auto fontCategoryCount = types::FontCategoryIterator().count();
   fontFamilies_.reserve(fontCategoryCount);
   fontStyles_.reserve(fontCategoryCount);
   fontPointSizes_.reserve(fontCategoryCount);

   for (const auto& fontCategory : types::FontCategoryIterator())
   {
      // Add font category to list view
      fontCategoryModel_->appendRow(new QStandardItem(
         QString::fromStdString(types::GetFontCategoryName(fontCategory))));

      // Create settings interface
      auto fontFamilyResult = fontFamilies_.emplace(
         fontCategory, settings::SettingsInterface<std::string> {});
      auto fontStyleResult = fontStyles_.emplace(
         fontCategory, settings::SettingsInterface<std::string> {});
      auto fontSizeResult = fontPointSizes_.emplace(
         fontCategory, settings::SettingsInterface<double> {});

      auto& fontFamily = (*fontFamilyResult.first).second;
      auto& fontStyle  = (*fontStyleResult.first).second;
      auto& fontSize   = (*fontSizeResult.first).second;

      // Add to settings list
      settings_.push_back(&fontFamily);
      settings_.push_back(&fontStyle);
      settings_.push_back(&fontSize);

      // Set settings variables
      fontFamily.SetSettingsVariable(textSettings.font_family(fontCategory));
      fontStyle.SetSettingsVariable(textSettings.font_style(fontCategory));
      fontSize.SetSettingsVariable(textSettings.font_point_size(fontCategory));
   }
   self_->ui->fontListView->setCurrentIndex(fontCategoryModel_->index(0, 0));
   SelectFontCategory(*types::FontCategoryIterator().begin());
   UpdateFontDisplayData();

   hoverTextWrap_.SetSettingsVariable(textSettings.hover_text_wrap());
   hoverTextWrap_.SetEditWidget(self_->ui->hoverTextWrapSpinBox);
   hoverTextWrap_.SetResetButton(self_->ui->resetHoverTextWrapButton);

   tooltipMethod_.SetSettingsVariable(textSettings.tooltip_method());
   SCWX_SETTINGS_COMBO_BOX(tooltipMethod_,
                           self_->ui->tooltipMethodComboBox,
                           types::TooltipMethodIterator(),
                           types::GetTooltipMethodName);
   tooltipMethod_.SetResetButton(self_->ui->resetTooltipMethodButton);

   placefileTextDropShadowEnabled_.SetSettingsVariable(
      textSettings.placefile_text_drop_shadow_enabled());
   placefileTextDropShadowEnabled_.SetEditWidget(
      self_->ui->placefileTextDropShadowCheckBox);

   radarSiteHoverTextEnabled_.SetSettingsVariable(
      textSettings.radar_site_hover_text_enabled());
   radarSiteHoverTextEnabled_.SetEditWidget(
      self_->ui->radarSiteHoverTextCheckBox);
}

void SettingsDialogImpl::SetupHotkeysTab()
{
   QVBoxLayout* layout = new QVBoxLayout(self_->ui->hotkeys);

   hotkeySettingsWidget_ = new HotkeySettingsWidget(self_->ui->hotkeys);
   layout->addWidget(hotkeySettingsWidget_);

   settingsPages_.push_back(hotkeySettingsWidget_);
}

QImage SettingsDialogImpl::GenerateColorTableImage(
   std::shared_ptr<common::ColorTable> colorTable,
   std::uint16_t                       min,
   std::uint16_t                       max,
   float                               offset,
   float                               scale)
{
   std::size_t width  = max - min + 1u;
   std::size_t height = 1u;
   QImage      image(static_cast<int>(width),
                static_cast<int>(height),
                QImage::Format::Format_ARGB32);

   for (std::size_t i = min; i <= max; ++i)
   {
      const float               value = (i - offset) / scale;
      boost::gil::rgba8_pixel_t pixel = colorTable->Color(value);
      image.setPixel(static_cast<int>(i - min),
                     0,
                     qRgba(static_cast<int>(pixel[0]),
                           static_cast<int>(pixel[1]),
                           static_cast<int>(pixel[2]),
                           static_cast<int>(pixel[3])));
   }

   return image;
}

void SettingsDialogImpl::LoadColorTablePreview(const std::string& key,
                                               const std::string& value,
                                               QLabel*            imageLabel)
{
   scwx::util::async(
      [key, value, imageLabel]()
      {
         std::unique_ptr<std::istream>       is = util::OpenFile(value);
         std::shared_ptr<common::ColorTable> colorTable =
            common::ColorTable::Load(*is);

         if (colorTable->IsValid())
         {
            auto&   conversions = kColorTableConversions_.at(key);
            QPixmap image =
               QPixmap::fromImage(GenerateColorTableImage(colorTable,
                                                          conversions.rangeMin,
                                                          conversions.rangeMax,
                                                          conversions.offset,
                                                          conversions.scale))
                  .scaled(64, 20);
            imageLabel->setPixmap(image);

            QMetaObject::invokeMethod(
               imageLabel, [imageLabel] { imageLabel->setVisible(true); });
         }
         else
         {
            imageLabel->clear();

            QMetaObject::invokeMethod(
               imageLabel, [imageLabel] { imageLabel->setVisible(false); });
         }
      });
}

void SettingsDialogImpl::UpdateRadarDialogLocation(const std::string& id)
{
   std::shared_ptr<config::RadarSite> radarSite = config::RadarSite::Get(id);

   if (radarSite != nullptr)
   {
      radarSiteDialog_->HandleMapUpdate(radarSite->latitude(),
                                        radarSite->longitude());
   }
}

void SettingsDialogImpl::UpdateAlertRadarDialogLocation(const std::string& id)
{
   std::shared_ptr<config::RadarSite> radarSite = config::RadarSite::Get(id);

   if (radarSite != nullptr)
   {
      alertAudioRadarSiteDialog_->HandleMapUpdate(radarSite->latitude(),
                                                  radarSite->longitude());
   }
}

QFont SettingsDialogImpl::GetSelectedFont()
{
   std::string fontFamily = fontFamilies_.at(selectedFontCategory_)
                               .GetSettingsVariable()
                               ->GetStagedOrValue();
   std::string fontStyle = fontStyles_.at(selectedFontCategory_)
                              .GetSettingsVariable()
                              ->GetStagedOrValue();
   units::font_size::points<double> fontSize {
      fontPointSizes_.at(selectedFontCategory_)
         .GetSettingsVariable()
         ->GetStagedOrValue()};

   QFont font = QFontDatabase::font(QString::fromStdString(fontFamily),
                                    QString::fromStdString(fontStyle),
                                    static_cast<int>(fontSize.value()));
   font.setPointSizeF(fontSize.value());
   font.setHintingPreference(QFont::HintingPreference::PreferNoHinting);

   return font;
}

void SettingsDialogImpl::SelectFontCategory(types::FontCategory fontCategory)
{
   selectedFontCategory_ = fontCategory;
}

void SettingsDialogImpl::UpdateFontDisplayData()
{
   QFont font = GetSelectedFont();

   self_->ui->fontNameLabel->setText(font.family());
   self_->ui->fontStyleLabel->setText(font.styleName());
   self_->ui->fontSizeLabel->setText(QString::number(font.pointSizeF()));

#if defined(__APPLE__)
   const units::font_size::points<double> fontSize {font.pointSizeF()};
   const units::font_size::pixels<double> fontPixels {fontSize};
   font.setPixelSize(static_cast<int>(fontPixels.value()));
#endif

   self_->ui->fontPreviewLabel->setFont(font);

   if (selectedFontCategory_ != types::FontCategory::Unknown)
   {
      auto& fontFamily = fontFamilies_.at(selectedFontCategory_);
      auto& fontStyle  = fontStyles_.at(selectedFontCategory_);
      auto& fontSize   = fontPointSizes_.at(selectedFontCategory_);

      self_->ui->resetFontButton->setVisible(!fontFamily.IsDefault() ||
                                             !fontStyle.IsDefault() ||
                                             !fontSize.IsDefault());
   }
}

void SettingsDialogImpl::ApplyChanges()
{
   logger_->info("Applying settings changes");

   bool committed    = false;
   bool themeUpdated = false;

   for (auto& setting : settings_)
   {
      const bool settingCommitted = setting->Commit();

      committed |= settingCommitted;
      if (settingCommitted && (setting == &theme_ || setting == &themeFile_))
      {
         themeUpdated = true;
      }
   }

   for (auto& page : settingsPages_)
   {
      committed |= page->CommitChanges();
   }

   if (themeUpdated)
   {
      main::ApplyTheme();
   }

   if (committed)
   {
      manager::SettingsManager::Instance().SaveSettings();
   }
}

void SettingsDialogImpl::DiscardChanges()
{
   logger_->info("Discarding settings changes");

   for (auto& setting : settings_)
   {
      setting->Reset();
   }

   for (auto& page : settingsPages_)
   {
      page->DiscardChanges();
   }
}

void SettingsDialogImpl::ResetToDefault()
{
   logger_->info("Restoring settings to default");

   for (auto& setting : settings_)
   {
      setting->StageDefault();
   }

   for (auto& page : settingsPages_)
   {
      page->ResetToDefault();
   }
}

std::string SettingsDialogImpl::RadarSiteLabel(
   std::shared_ptr<config::RadarSite>& radarSite)
{
   return fmt::format("{} ({})", radarSite->id(), radarSite->location_name());
}

// NOLINTEND(cppcoreguidelines-owning-memory)

} // namespace scwx::qt::ui
