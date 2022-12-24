#include "settings_dialog.hpp"
#include "ui_settings_dialog.h"

#include <scwx/awips/phenomenon.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/ui/radar_site_dialog.hpp>
#include <scwx/util/logger.hpp>

#include <format>

#include <QFileDialog>
#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::settings_dialog";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const std::array<awips::Phenomenon, 5> kAlertPhenomena_ {
   awips::Phenomenon::FlashFlood,
   awips::Phenomenon::Marine,
   awips::Phenomenon::SevereThunderstorm,
   awips::Phenomenon::SnowSquall,
   awips::Phenomenon::Tornado};

static const std::array<std::pair<std::string, std::string>, 17>
   kColorTableTypes_ {std::pair {"BR", "BR"},
                      std::pair {"BV", "BV"},
                      std::pair {"SW", "SW"},
                      std::pair {"ZDR", "ZDR"},
                      std::pair {"PHI2", "PHI2"},
                      std::pair {"CC", "CC"},
                      std::pair {"DOD", "DOD"},
                      std::pair {"DSD", "DSD"},
                      std::pair {"ET", "ET"},
                      std::pair {"OHP", "OHP"},
                      std::pair {"OHPIN", "OHPIN"},
                      std::pair {"PHI3", "PHI3"},
                      std::pair {"SRV", "SRV"},
                      std::pair {"STP", "STP"},
                      std::pair {"STPIN", "STPIN"},
                      std::pair {"VIL", "VIL"},
                      std::pair {"???", "Default"}};

class SettingsDialogImpl
{
public:
   explicit SettingsDialogImpl(SettingsDialog* self) :
       self_ {self},
       radarSiteDialog_ {new RadarSiteDialog(self)},
       settings_ {std::initializer_list<settings::SettingsInterfaceBase*> {
          &defaultRadarSite_,
          &fontSizes_,
          &gridWidth_,
          &gridHeight_,
          &mapboxApiKey_,
          &debugEnabled_}}
   {
   }
   ~SettingsDialogImpl() = default;

   void ConnectSignals();
   void SetupGeneralTab();
   void SetupPalettesColorTablesTab();
   void SetupPalettesAlertsTab();

   void UpdateRadarDialogLocation(const std::string& id);

   void ApplyChanges();
   void DiscardChanges();
   void ResetToDefault();

   static std::string
   RadarSiteLabel(std::shared_ptr<config::RadarSite>& radarSite);

   SettingsDialog*  self_;
   RadarSiteDialog* radarSiteDialog_;

   settings::SettingsInterface<std::string>               defaultRadarSite_ {};
   settings::SettingsInterface<std::vector<std::int64_t>> fontSizes_ {};
   settings::SettingsInterface<std::int64_t>              gridWidth_ {};
   settings::SettingsInterface<std::int64_t>              gridHeight_ {};
   settings::SettingsInterface<std::string>               mapboxApiKey_ {};
   settings::SettingsInterface<bool>                      debugEnabled_ {};

   std::unordered_map<std::string, settings::SettingsInterface<std::string>>
      colorTables_ {};

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
         }
      });
}

void SettingsDialogImpl::SetupGeneralTab()
{
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

   settings::GeneralSettings& generalSettings =
      manager::SettingsManager::general_settings();

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

   fontSizes_.SetSettingsVariable(generalSettings.font_sizes());
   fontSizes_.SetEditWidget(self_->ui->fontSizesLineEdit);
   fontSizes_.SetResetButton(self_->ui->resetFontSizesButton);

   gridWidth_.SetSettingsVariable(generalSettings.grid_width());
   gridWidth_.SetEditWidget(self_->ui->gridWidthSpinBox);
   gridWidth_.SetResetButton(self_->ui->resetGridWidthButton);

   gridHeight_.SetSettingsVariable(generalSettings.grid_height());
   gridHeight_.SetEditWidget(self_->ui->gridHeightSpinBox);
   gridHeight_.SetResetButton(self_->ui->resetGridHeightButton);

   mapboxApiKey_.SetSettingsVariable(generalSettings.mapbox_api_key());
   mapboxApiKey_.SetEditWidget(self_->ui->mapboxApiKeyLineEdit);
   mapboxApiKey_.SetResetButton(self_->ui->resetMapboxApiKeyButton);

   debugEnabled_.SetSettingsVariable(generalSettings.debug_enabled());
   debugEnabled_.SetEditWidget(self_->ui->debugEnabledCheckBox);
}

void SettingsDialogImpl::SetupPalettesColorTablesTab()
{
   settings::PaletteSettings& paletteSettings =
      manager::SettingsManager::palette_settings();

   // Palettes > Color Tables
   QGridLayout* colorTableLayout =
      reinterpret_cast<QGridLayout*>(self_->ui->colorTableContents->layout());

   int colorTableRow = 0;
   for (auto& colorTableType : kColorTableTypes_)
   {
      QLineEdit*   lineEdit       = new QLineEdit(self_);
      QToolButton* openFileButton = new QToolButton(self_);
      QToolButton* resetButton    = new QToolButton(self_);

      openFileButton->setText(QObject::tr("..."));

      resetButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
      resetButton->setVisible(false);

      colorTableLayout->addWidget(
         new QLabel(colorTableType.second.c_str(), self_), colorTableRow, 0);
      colorTableLayout->addWidget(lineEdit, colorTableRow, 1);
      colorTableLayout->addWidget(openFileButton, colorTableRow, 2);
      colorTableLayout->addWidget(resetButton, colorTableRow, 3);
      ++colorTableRow;

      // Create settings interface
      auto result = colorTables_.emplace(
         colorTableType.first, settings::SettingsInterface<std::string> {});
      auto& pair       = *result.first;
      auto& colorTable = pair.second;

      // Add to settings list
      settings_.push_back(&colorTable);

      colorTable.SetSettingsVariable(
         paletteSettings.palette(colorTableType.first));
      colorTable.SetEditWidget(lineEdit);
      colorTable.SetResetButton(resetButton);

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

                                // textEdit does not emit the textEdited signal
                                emit lineEdit->textEdited(path);
                             });

            dialog->open();
         });
   }
}

void SettingsDialogImpl::SetupPalettesAlertsTab()
{
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
   alertsLayout->addWidget(activeLabel, 0, 1, 1, 3);
   alertsLayout->addWidget(inactiveLabel, 0, 4, 1, 3);

   int alertsRow = 1;
   for (auto& phenomenon : kAlertPhenomena_)
   {
      QFrame* activeFrame   = new QFrame(self_);
      QFrame* inactiveFrame = new QFrame(self_);

      QToolButton* activeButton   = new QToolButton(self_);
      QToolButton* inactiveButton = new QToolButton(self_);
      QToolButton* resetButton    = new QToolButton(self_);

      activeFrame->setMinimumWidth(24);
      inactiveFrame->setMinimumWidth(24);

      activeButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/palette-solid.svg"});
      inactiveButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/palette-solid.svg"});
      resetButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});

      resetButton->setVisible(false);

      alertsLayout->addWidget(
         new QLabel(QObject::tr(awips::GetPhenomenonText(phenomenon).c_str()),
                    self_),
         alertsRow,
         0);
      alertsLayout->addWidget(activeFrame, alertsRow, 1);
      alertsLayout->addWidget(new QLineEdit(self_), alertsRow, 2);
      alertsLayout->addWidget(activeButton, alertsRow, 3);
      alertsLayout->addWidget(inactiveFrame, alertsRow, 4);
      alertsLayout->addWidget(new QLineEdit(self_), alertsRow, 5);
      alertsLayout->addWidget(inactiveButton, alertsRow, 6);
      alertsLayout->addWidget(resetButton, alertsRow, 7);
      ++alertsRow;
   }
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
      manager::SettingsManager::SaveSettings();
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
   return std::format("{} ({})", radarSite->id(), radarSite->location_name());
}

} // namespace ui
} // namespace qt
} // namespace scwx
