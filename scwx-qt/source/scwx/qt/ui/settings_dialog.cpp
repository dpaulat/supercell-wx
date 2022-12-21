#include "settings_dialog.hpp"
#include "ui_settings_dialog.h"

#include <scwx/awips/phenomenon.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/settings_interface.hpp>

#include <format>

#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

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
   explicit SettingsDialogImpl(SettingsDialog* self) : self_ {self} {}
   ~SettingsDialogImpl() = default;

   void SetupGeneralTab();
   void SetupPalettesColorTablesTab();
   void SetupPalettesAlertsTab();

   SettingsDialog* self_;

   settings::SettingsInterface<std::string> defaultRadarSite_ {};
   settings::SettingsInterface<int64_t>     gridWidth_ {};
   settings::SettingsInterface<int64_t>     gridHeight_ {};
   settings::SettingsInterface<std::string> mapboxApiKey_ {};
   settings::SettingsInterface<bool>        debugEnabled_ {};

   std::unordered_map<std::string, settings::SettingsInterface<std::string>>
      colorTables_ {};
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

   connect(ui->listWidget,
           &QListWidget::currentRowChanged,
           ui->stackedWidget,
           &QStackedWidget::setCurrentIndex);
}

SettingsDialog::~SettingsDialog()
{
   delete ui;
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
      QString text = QString::fromStdString(
         std::format("{} ({})", radarSite->id(), radarSite->location_name()));
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
         return std::format(
            "{} ({})", radarSite->id(), radarSite->location_name());
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
      QLineEdit*   lineEdit    = new QLineEdit(self_);
      QToolButton* resetButton = new QToolButton(self_);

      resetButton->setIcon(
         QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
      resetButton->setVisible(false);

      colorTableLayout->addWidget(
         new QLabel(colorTableType.second.c_str(), self_), colorTableRow, 0);
      colorTableLayout->addWidget(lineEdit, colorTableRow, 1);
      colorTableLayout->addWidget(new QToolButton(self_), colorTableRow, 2);
      colorTableLayout->addWidget(resetButton, colorTableRow, 3);
      ++colorTableRow;

      // Create settings interface
      auto result = colorTables_.emplace(
         colorTableType.first, settings::SettingsInterface<std::string> {});
      auto& pair       = *result.first;
      auto& colorTable = pair.second;

      colorTable.SetSettingsVariable(
         paletteSettings.palette(colorTableType.first));
      colorTable.SetEditWidget(lineEdit);
      colorTable.SetResetButton(resetButton);
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

} // namespace ui
} // namespace qt
} // namespace scwx
