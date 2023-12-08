#include "settings_dialog.hpp"
#include "ui_settings_dialog.h"

#include <scwx/awips/phenomenon.hpp>
#include <scwx/common/color_table.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/manager/media_manager.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/settings/audio_settings.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/types/alert_types.hpp>
#include <scwx/qt/types/font_types.hpp>
#include <scwx/qt/types/location_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/types/text_types.hpp>
#include <scwx/qt/ui/county_dialog.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
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
#include <QStandardItemModel>
#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::settings_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

struct ColorTableConversions
{
   uint16_t rangeMin {0u};
   uint16_t rangeMax {255u};
   float    offset {0.0f};
   float    scale {1.0f};
};

static const std::array<std::pair<std::string, std::string>, 15>
   kColorTableTypes_ {std::pair {"BR", "BR"},
                      std::pair {"BV", "BV"},
                      std::pair {"SW", "SW"},
                      std::pair {"ZDR", "ZDR"},
                      std::pair {"PHI2", "KDP (L2)"},
                      std::pair {"CC", "CC"},
                      std::pair {"DOD", "DOD"},
                      std::pair {"DSD", "DSD"},
                      std::pair {"ET", "ET"},
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
                            {"OHP", {0u, 255u, 0.0f, 2.5f}},
                            {"PHI3", {0u, 255u, 43.0f, 20.0f}},
                            {"SRV", {0u, 255u, 128.0f, 2.0f}},
                            {"STP", {0u, 255u, 0.0f, 1.25f}},
                            {"VIL", {0u, 255u, 1.0f, 2.5f}},
                            {"???", {0u, 15u, 0.0f, 1.0f}}};

#define SCWX_ENUM_MAP_FROM_VALUE(Type, Iterator, ToName)                       \
   [](const std::string& text) -> std::string                                  \
   {                                                                           \
      for (Type enumValue : Iterator)                                          \
      {                                                                        \
         const std::string enumName = ToName(enumValue);                       \
                                                                               \
         if (boost::iequals(text, enumName))                                   \
         {                                                                     \
            /* Return label */                                                 \
            return enumName;                                                   \
         }                                                                     \
      }                                                                        \
                                                                               \
      /* Label not found, return unknown */                                    \
      return "?";                                                              \
   }

class SettingsDialogImpl
{
public:
   explicit SettingsDialogImpl(SettingsDialog* self) :
       self_ {self},
       radarSiteDialog_ {new RadarSiteDialog(self)},
       countyDialog_ {new CountyDialog(self)},
       fontDialog_ {new QFontDialog(self)},
       fontCategoryModel_ {new QStandardItemModel(self)},
       settings_ {std::initializer_list<settings::SettingsInterfaceBase*> {
          &defaultRadarSite_,
          &gridWidth_,
          &gridHeight_,
          &mapProvider_,
          &mapboxApiKey_,
          &mapTilerApiKey_,
          &theme_,
          &defaultAlertAction_,
          &antiAliasingEnabled_,
          &updateNotificationsEnabled_,
          &debugEnabled_,
          &alertAudioSoundFile_,
          &alertAudioLocationMethod_,
          &alertAudioLatitude_,
          &alertAudioLongitude_,
          &alertAudioCounty_,
          &hoverTextWrap_,
          &tooltipMethod_,
          &placefileTextDropShadowEnabled_}}
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
   void SetupAudioTab();
   void SetupTextTab();

   void ShowColorDialog(QLineEdit* lineEdit, QFrame* frame = nullptr);
   void UpdateRadarDialogLocation(const std::string& id);

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
   static void SetBackgroundColor(const std::string& value, QFrame* frame);

   SettingsDialog*  self_;
   RadarSiteDialog* radarSiteDialog_;
   CountyDialog*    countyDialog_;
   QFontDialog*     fontDialog_;

   QStandardItemModel* fontCategoryModel_;

   types::FontCategory selectedFontCategory_ {types::FontCategory::Unknown};

   std::shared_ptr<manager::MediaManager> mediaManager_ {
      manager::MediaManager::Instance()};
   std::shared_ptr<manager::PositionManager> positionManager_ {
      manager::PositionManager::Instance()};

   settings::SettingsInterface<std::string>  defaultRadarSite_ {};
   settings::SettingsInterface<std::int64_t> gridWidth_ {};
   settings::SettingsInterface<std::int64_t> gridHeight_ {};
   settings::SettingsInterface<std::string>  mapProvider_ {};
   settings::SettingsInterface<std::string>  mapboxApiKey_ {};
   settings::SettingsInterface<std::string>  mapTilerApiKey_ {};
   settings::SettingsInterface<std::string>  defaultAlertAction_ {};
   settings::SettingsInterface<std::string>  theme_ {};
   settings::SettingsInterface<bool>         antiAliasingEnabled_ {};
   settings::SettingsInterface<bool>         updateNotificationsEnabled_ {};
   settings::SettingsInterface<bool>         debugEnabled_ {};

   std::unordered_map<std::string, settings::SettingsInterface<std::string>>
      colorTables_ {};
   std::unordered_map<awips::Phenomenon,
                      settings::SettingsInterface<std::string>>
      activeAlertColors_ {};
   std::unordered_map<awips::Phenomenon,
                      settings::SettingsInterface<std::string>>
      inactiveAlertColors_ {};

   settings::SettingsInterface<std::string> alertAudioSoundFile_ {};
   settings::SettingsInterface<std::string> alertAudioLocationMethod_ {};
   settings::SettingsInterface<double>      alertAudioLatitude_ {};
   settings::SettingsInterface<double>      alertAudioLongitude_ {};
   settings::SettingsInterface<std::string> alertAudioCounty_ {};

   std::unordered_map<awips::Phenomenon, settings::SettingsInterface<bool>>
      alertAudioEnabled_ {};

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

   std::vector<settings::SettingsInterfaceBase*> settings_;
};

SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<SettingsDialogImpl>(this)},
    ui(new Ui::SettingsDialog)
{
   ui->setupUi(this);

   // General
   p->SetupGeneralTab();

   // Palettes > Color Tables
   p->SetupPalettesColorTablesTab();

   // Palettes > Alerts
   p->SetupPalettesAlertsTab();

   // Audio
   p->SetupAudioTab();

   // Text
   p->SetupTextTab();

   p->ConnectSignals();
}

SettingsDialog::~SettingsDialog()
{
   delete ui;
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

   for (const auto& uiStyle : types::UiStyleIterator())
   {
      self_->ui->themeComboBox->addItem(
         QString::fromStdString(types::GetUiStyleName(uiStyle)));
   }

   theme_.SetSettingsVariable(generalSettings.theme());
   theme_.SetMapFromValueFunction(
      [](const std::string& text) -> std::string
      {
         for (types::UiStyle uiStyle : types::UiStyleIterator())
         {
            const std::string uiStyleName = types::GetUiStyleName(uiStyle);

            if (boost::iequals(text, uiStyleName))
            {
               // Return UI style label
               return uiStyleName;
            }
         }

         // UI style label not found, return unknown
         return "?";
      });
   theme_.SetMapToValueFunction(
      [](std::string text) -> std::string
      {
         // Convert label to lower case and return
         boost::to_lower(text);
         return text;
      });
   theme_.SetEditWidget(self_->ui->themeComboBox);
   theme_.SetResetButton(self_->ui->resetThemeButton);

   auto radarSites = config::RadarSite::GetAll();

   // Sort radar sites by ID
   std::sort(radarSites.begin(),
             radarSites.end(),
             [](const std::shared_ptr<config::RadarSite>& a,
                const std::shared_ptr<config::RadarSite>& b)
             { return a->id() < b->id(); });

   // Add sorted radar sites
   for (std::shared_ptr<config::RadarSite>& radarSite : radarSites)
   {
      QString text = QString::fromStdString(RadarSiteLabel(radarSite));
      self_->ui->radarSiteComboBox->addItem(text);
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
         size_t pos = text.rfind(" (");

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

   for (const auto& mapProvider : map::MapProviderIterator())
   {
      self_->ui->mapProviderComboBox->addItem(
         QString::fromStdString(map::GetMapProviderName(mapProvider)));
   }

   mapProvider_.SetSettingsVariable(generalSettings.map_provider());
   mapProvider_.SetMapFromValueFunction(
      [](const std::string& text) -> std::string
      {
         for (map::MapProvider mapProvider : map::MapProviderIterator())
         {
            const std::string mapProviderName =
               map::GetMapProviderName(mapProvider);

            if (boost::iequals(text, mapProviderName))
            {
               // Return map provider label
               return mapProviderName;
            }
         }

         // Map provider label not found, return unknown
         return "?";
      });
   mapProvider_.SetMapToValueFunction(
      [](std::string text) -> std::string
      {
         // Convert label to lower case and return
         boost::to_lower(text);
         return text;
      });
   mapProvider_.SetEditWidget(self_->ui->mapProviderComboBox);
   mapProvider_.SetResetButton(self_->ui->resetMapProviderButton);

   mapboxApiKey_.SetSettingsVariable(generalSettings.mapbox_api_key());
   mapboxApiKey_.SetEditWidget(self_->ui->mapboxApiKeyLineEdit);
   mapboxApiKey_.SetResetButton(self_->ui->resetMapboxApiKeyButton);

   mapTilerApiKey_.SetSettingsVariable(generalSettings.maptiler_api_key());
   mapTilerApiKey_.SetEditWidget(self_->ui->mapTilerApiKeyLineEdit);
   mapTilerApiKey_.SetResetButton(self_->ui->resetMapTilerApiKeyButton);

   for (const auto& alertAction : types::AlertActionIterator())
   {
      self_->ui->defaultAlertActionComboBox->addItem(
         QString::fromStdString(types::GetAlertActionName(alertAction)));
   }

   defaultAlertAction_.SetSettingsVariable(
      generalSettings.default_alert_action());
   defaultAlertAction_.SetMapFromValueFunction(
      [](const std::string& text) -> std::string
      {
         for (types::AlertAction alertAction : types::AlertActionIterator())
         {
            const std::string alertActionName =
               types::GetAlertActionName(alertAction);

            if (boost::iequals(text, alertActionName))
            {
               // Return alert action label
               return alertActionName;
            }
         }

         // Alert action label not found, return unknown
         return "?";
      });
   defaultAlertAction_.SetMapToValueFunction(
      [](std::string text) -> std::string
      {
         // Convert label to lower case and return
         boost::to_lower(text);
         return text;
      });
   defaultAlertAction_.SetEditWidget(self_->ui->defaultAlertActionComboBox);
   defaultAlertAction_.SetResetButton(self_->ui->resetDefaultAlertActionButton);

   antiAliasingEnabled_.SetSettingsVariable(
      generalSettings.anti_aliasing_enabled());
   antiAliasingEnabled_.SetEditWidget(self_->ui->antiAliasingEnabledCheckBox);

   updateNotificationsEnabled_.SetSettingsVariable(
      generalSettings.update_notifications_enabled());
   updateNotificationsEnabled_.SetEditWidget(
      self_->ui->enableUpdateNotificationsCheckBox);

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
                             [this, lineEdit](const QString& file)
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
   settings::PaletteSettings& paletteSettings =
      settings::PaletteSettings::Instance();

   // Palettes > Alerts
   QGridLayout* alertsLayout =
      reinterpret_cast<QGridLayout*>(self_->ui->alertsFrame->layout());

   QLabel* phenomenonLabel = new QLabel(QObject::tr("Phenomenon"), self_);
   QLabel* activeLabel     = new QLabel(QObject::tr("Active"), self_);
   QLabel* inactiveLabel   = new QLabel(QObject::tr("Inactive"), self_);

   QFont boldFont;
   boldFont.setBold(true);
   phenomenonLabel->setFont(boldFont);
   activeLabel->setFont(boldFont);
   inactiveLabel->setFont(boldFont);

   alertsLayout->addWidget(phenomenonLabel, 0, 0);
   alertsLayout->addWidget(activeLabel, 0, 1, 1, 4);
   alertsLayout->addWidget(inactiveLabel, 0, 5, 1, 4);

   int alertsRow = 1;
   for (auto& phenomenon : settings::PaletteSettings::alert_phenomena())
   {
      QFrame* activeFrame   = new QFrame(self_);
      QFrame* inactiveFrame = new QFrame(self_);

      QLineEdit* activeEdit   = new QLineEdit(self_);
      QLineEdit* inactiveEdit = new QLineEdit(self_);

      QToolButton* activeButton        = new QToolButton(self_);
      QToolButton* inactiveButton      = new QToolButton(self_);
      QToolButton* activeResetButton   = new QToolButton(self_);
      QToolButton* inactiveResetButton = new QToolButton(self_);

      activeFrame->setMinimumHeight(24);
      activeFrame->setMinimumWidth(24);
      activeFrame->setFrameShape(QFrame::Shape::Box);
      activeFrame->setFrameShadow(QFrame::Shadow::Plain);
      inactiveFrame->setMinimumHeight(24);
      inactiveFrame->setMinimumWidth(24);
      inactiveFrame->setFrameShape(QFrame::Shape::Box);
      inactiveFrame->setFrameShadow(QFrame::Shadow::Plain);

      activeButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/palette-solid.svg"});
      inactiveButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/palette-solid.svg"});
      activeResetButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
      inactiveResetButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});

      alertsLayout->addWidget(
         new QLabel(QObject::tr(awips::GetPhenomenonText(phenomenon).c_str()),
                    self_),
         alertsRow,
         0);
      alertsLayout->addWidget(activeFrame, alertsRow, 1);
      alertsLayout->addWidget(activeEdit, alertsRow, 2);
      alertsLayout->addWidget(activeButton, alertsRow, 3);
      alertsLayout->addWidget(activeResetButton, alertsRow, 4);
      alertsLayout->addWidget(inactiveFrame, alertsRow, 5);
      alertsLayout->addWidget(inactiveEdit, alertsRow, 6);
      alertsLayout->addWidget(inactiveButton, alertsRow, 7);
      alertsLayout->addWidget(inactiveResetButton, alertsRow, 8);
      ++alertsRow;

      // Create settings interface
      auto activeResult = activeAlertColors_.emplace(
         phenomenon, settings::SettingsInterface<std::string> {});
      auto inactiveResult = inactiveAlertColors_.emplace(
         phenomenon, settings::SettingsInterface<std::string> {});
      auto& activeColor   = activeResult.first->second;
      auto& inactiveColor = inactiveResult.first->second;

      // Add to settings list
      settings_.push_back(&activeColor);
      settings_.push_back(&inactiveColor);

      auto& activeSetting   = paletteSettings.alert_color(phenomenon, true);
      auto& inactiveSetting = paletteSettings.alert_color(phenomenon, false);

      activeColor.SetSettingsVariable(activeSetting);
      activeColor.SetEditWidget(activeEdit);
      activeColor.SetResetButton(activeResetButton);

      inactiveColor.SetSettingsVariable(inactiveSetting);
      inactiveColor.SetEditWidget(inactiveEdit);
      inactiveColor.SetResetButton(inactiveResetButton);

      SetBackgroundColor(activeSetting.GetValue(), activeFrame);
      SetBackgroundColor(inactiveSetting.GetValue(), inactiveFrame);

      activeSetting.RegisterValueStagedCallback(
         [activeFrame](const std::string& value)
         { SetBackgroundColor(value, activeFrame); });
      inactiveSetting.RegisterValueStagedCallback(
         [inactiveFrame](const std::string& value)
         { SetBackgroundColor(value, inactiveFrame); });

      QObject::connect(activeButton,
                       &QAbstractButton::clicked,
                       self_,
                       [=, this]()
                       { ShowColorDialog(activeEdit, activeFrame); });
      QObject::connect(inactiveButton,
                       &QAbstractButton::clicked,
                       self_,
                       [=, this]()
                       { ShowColorDialog(inactiveEdit, inactiveFrame); });
   }
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
         bool countyEntryEnabled =
            locationMethod == types::LocationMethod::County;

         self_->ui->alertAudioLatitudeSpinBox->setEnabled(
            coordinateEntryEnabled);
         self_->ui->alertAudioLongitudeSpinBox->setEnabled(
            coordinateEntryEnabled);
         self_->ui->resetAlertAudioLatitudeButton->setEnabled(
            coordinateEntryEnabled);
         self_->ui->resetAlertAudioLongitudeButton->setEnabled(
            coordinateEntryEnabled);

         self_->ui->alertAudioCountyLineEdit->setEnabled(countyEntryEnabled);
         self_->ui->alertAudioCountySelectButton->setEnabled(
            countyEntryEnabled);
         self_->ui->resetAlertAudioCountyButton->setEnabled(countyEntryEnabled);
      });

   settings::AudioSettings& audioSettings = settings::AudioSettings::Instance();

   alertAudioSoundFile_.SetSettingsVariable(audioSettings.alert_sound_file());
   alertAudioSoundFile_.SetEditWidget(self_->ui->alertAudioSoundLineEdit);
   alertAudioSoundFile_.SetResetButton(self_->ui->resetAlertAudioSoundButton);

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

   for (const auto& locationMethod : types::LocationMethodIterator())
   {
      self_->ui->alertAudioLocationMethodComboBox->addItem(
         QString::fromStdString(types::GetLocationMethodName(locationMethod)));
   }

   alertAudioLocationMethod_.SetSettingsVariable(
      audioSettings.alert_location_method());
   alertAudioLocationMethod_.SetMapFromValueFunction(
      SCWX_ENUM_MAP_FROM_VALUE(types::LocationMethod,
                               types::LocationMethodIterator(),
                               types::GetLocationMethodName));
   alertAudioLocationMethod_.SetMapToValueFunction(
      [](std::string text) -> std::string
      {
         // Convert label to lower case and return
         boost::to_lower(text);
         return text;
      });
   alertAudioLocationMethod_.SetEditWidget(
      self_->ui->alertAudioLocationMethodComboBox);
   alertAudioLocationMethod_.SetResetButton(
      self_->ui->resetAlertAudioLocationMethodButton);

   alertAudioLatitude_.SetSettingsVariable(audioSettings.alert_latitude());
   alertAudioLatitude_.SetEditWidget(self_->ui->alertAudioLatitudeSpinBox);
   alertAudioLatitude_.SetResetButton(self_->ui->resetAlertAudioLatitudeButton);

   alertAudioLongitude_.SetSettingsVariable(audioSettings.alert_longitude());
   alertAudioLongitude_.SetEditWidget(self_->ui->alertAudioLongitudeSpinBox);
   alertAudioLongitude_.SetResetButton(
      self_->ui->resetAlertAudioLongitudeButton);

   auto alertAudioLayout =
      static_cast<QGridLayout*>(self_->ui->alertAudioGroupBox->layout());

   for (const auto& phenomenon : types::GetAlertAudioPhenomena())
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
}

void SettingsDialogImpl::SetupTextTab()
{
   settings::TextSettings& textSettings = settings::TextSettings::Instance();

   self_->ui->fontListView->setModel(fontCategoryModel_);
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

   for (const auto& tooltipMethod : types::TooltipMethodIterator())
   {
      self_->ui->tooltipMethodComboBox->addItem(
         QString::fromStdString(types::GetTooltipMethodName(tooltipMethod)));
   }

   tooltipMethod_.SetSettingsVariable(textSettings.tooltip_method());
   tooltipMethod_.SetMapFromValueFunction(
      [](const std::string& text) -> std::string
      {
         for (types::TooltipMethod tooltipMethod :
              types::TooltipMethodIterator())
         {
            const std::string tooltipMethodName =
               types::GetTooltipMethodName(tooltipMethod);

            if (boost::iequals(text, tooltipMethodName))
            {
               // Return tooltip method label
               return tooltipMethodName;
            }
         }

         // Tooltip method label not found, return unknown
         return "?";
      });
   tooltipMethod_.SetMapToValueFunction(
      [](std::string text) -> std::string
      {
         // Convert label to lower case and return
         boost::to_lower(text);
         return text;
      });
   tooltipMethod_.SetEditWidget(self_->ui->tooltipMethodComboBox);
   tooltipMethod_.SetResetButton(self_->ui->resetTooltipMethodButton);

   placefileTextDropShadowEnabled_.SetSettingsVariable(
      textSettings.placefile_text_drop_shadow_enabled());
   placefileTextDropShadowEnabled_.SetEditWidget(
      self_->ui->placefileTextDropShadowCheckBox);
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

void SettingsDialogImpl::ShowColorDialog(QLineEdit* lineEdit, QFrame* frame)
{
   QColorDialog* dialog = new QColorDialog(self_);

   dialog->setAttribute(Qt::WA_DeleteOnClose);
   dialog->setOption(QColorDialog::ColorDialogOption::ShowAlphaChannel);

   QColor initialColor(lineEdit->text());
   if (initialColor.isValid())
   {
      dialog->setCurrentColor(initialColor);
   }

   QObject::connect(
      dialog,
      &QColorDialog::colorSelected,
      self_,
      [this, lineEdit, frame](const QColor& color)
      {
         QString colorName = color.name(QColor::NameFormat::HexArgb);

         logger_->info("Selected color: {}", colorName.toStdString());
         lineEdit->setText(colorName);

         // setText does not emit the textEdited signal
         Q_EMIT lineEdit->textEdited(colorName);
      });

   dialog->open();
}

void SettingsDialogImpl::SetBackgroundColor(const std::string& value,
                                            QFrame*            frame)
{
   frame->setStyleSheet(
      QString::fromStdString(fmt::format("background-color: {}", value)));
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

   bool committed = false;

   for (auto& setting : settings_)
   {
      committed |= setting->Commit();
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
}

void SettingsDialogImpl::ResetToDefault()
{
   logger_->info("Restoring settings to default");

   for (auto& setting : settings_)
   {
      setting->StageDefault();
   }
}

std::string SettingsDialogImpl::RadarSiteLabel(
   std::shared_ptr<config::RadarSite>& radarSite)
{
   return fmt::format("{} ({})", radarSite->id(), radarSite->location_name());
}

} // namespace ui
} // namespace qt
} // namespace scwx
