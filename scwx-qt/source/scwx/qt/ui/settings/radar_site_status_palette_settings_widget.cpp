#include <scwx/qt/ui/settings/radar_site_status_palette_settings_widget.hpp>
#include <scwx/qt/ui/edit_button_dialog.hpp>
#include <scwx/qt/ui/widgets/imgui_button.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/types/radar_site_types.hpp>
#include <scwx/qt/util/color.hpp>

#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

#include <boost/signals2.hpp>

namespace scwx::qt::ui
{

// Extensive usage of "new" with Qt-managed objects
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

static const std::string logPrefix_ =
   "scwx::qt::ui::settings::radar_site_status_palette_settings_widget";

class RadarSiteStatusPaletteSettingsWidget::Impl
{
public:
   explicit Impl(RadarSiteStatusPaletteSettingsWidget* self) :
       self_ {self}, editButtonDialog_ {new EditButtonDialog(self)}
   {
      SetupUi();
      ConnectSignals();
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void     AddStatusButton(types::RadarSiteStatus    status,
                            settings::ButtonSettings& buttonSettings,
                            QGridLayout*              layout,
                            int                       row);
   QWidget* CreateStackedWidgetPage(awips::Phenomenon phenomenon);
   void     ConnectSignals();
   void     SetupUi();

   RadarSiteStatusPaletteSettingsWidget* self_;

   EditButtonDialog*         editButtonDialog_;
   settings::ButtonSettings* activeButtonSettings_ {nullptr};

   std::vector<boost::signals2::scoped_connection> connections_ {};
};

RadarSiteStatusPaletteSettingsWidget::RadarSiteStatusPaletteSettingsWidget(
   QWidget* parent) :
    SettingsPageWidget(parent), p {std::make_shared<Impl>(this)}
{
}

RadarSiteStatusPaletteSettingsWidget::~RadarSiteStatusPaletteSettingsWidget() =
   default;

void RadarSiteStatusPaletteSettingsWidget::Impl::SetupUi()
{
   auto& paletteSettings = settings::PaletteSettings::Instance();

   // Setup phenomenon index pane
   auto radarSiteStatusLabel = new QLabel(tr("Radar Site Status:"), self_);

   int row = 0;

   // Create primary widget layout
   auto gridLayout = new QGridLayout(self_);
   gridLayout->setContentsMargins(0, 0, 0, 0);
   gridLayout->addWidget(radarSiteStatusLabel, row++, 0);

   // Setup stacked widget
   for (auto radarSiteStatus : types::RadarSiteStatusIterator())
   {
      AddStatusButton(
         radarSiteStatus,
         paletteSettings.radar_site_status_palette(radarSiteStatus).button(),
         gridLayout,
         row++);
   }

   auto spacer =
      new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
   gridLayout->addItem(spacer, row, 0);

   self_->setLayout(gridLayout);
}

void RadarSiteStatusPaletteSettingsWidget::Impl::ConnectSignals()
{
   connect(editButtonDialog_,
           &EditButtonDialog::accepted,
           self_,
           [this]()
           {
              // If the active button settings was set
              if (activeButtonSettings_ != nullptr)
              {
                 // Update the active button settings with selected button
                 // settings
                 activeButtonSettings_->StageValues(
                    editButtonDialog_->button_color(),
                    editButtonDialog_->hover_color(),
                    editButtonDialog_->active_color());

                 // Reset the active button settings
                 activeButtonSettings_ = nullptr;
              }
           });
}

void RadarSiteStatusPaletteSettingsWidget::Impl::AddStatusButton(
   types::RadarSiteStatus    status,
   settings::ButtonSettings& buttonSettings,
   QGridLayout*              layout,
   int                       row)
{
   const std::string& longName = types::GetRadarSiteStatusLongName(status);
   const std::string& description =
      types::GetRadarSiteStatusDescription(status);

   auto font =
      manager::FontManager::Instance().GetQFont(types::FontCategory::Default);

   auto toolButton = new QToolButton(self_);
   toolButton->setText(tr("..."));

   auto nameLabel = new QLabel(tr(longName.c_str()), self_);
   nameLabel->setToolTip(tr(description.c_str()));

   auto imguiButton = new ImGuiButton(self_);
   imguiButton->set_button_settings(buttonSettings);
   imguiButton->setFont(font);
   imguiButton->setText(tr("BUTTON TEXT"));

   auto resetButton = new QToolButton(self_);
   resetButton->setIcon(
      QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
   resetButton->setVisible(!buttonSettings.IsDefaultStaged());

   auto spacer =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

   layout->addWidget(nameLabel, row, 0);
   layout->addWidget(imguiButton, row, 1);
   layout->addWidget(toolButton, row, 2);
   layout->addWidget(resetButton, row, 3);
   layout->addItem(spacer, row, 4);

   self_->AddSettingsCategory(&buttonSettings);

   connect(toolButton,
           &QAbstractButton::clicked,
           self_,
           [this, &buttonSettings]()
           {
              // Set the active line label for when the dialog is finished
              activeButtonSettings_ = &buttonSettings;

              // Initialize dialog with current line settings
              editButtonDialog_->Initialize(
                 buttonSettings.GetActiveColorRgba8(),
                 buttonSettings.GetButtonColorRgba8(),
                 buttonSettings.GetHoverColorRgba8());

              // Show the dialog
              editButtonDialog_->show();
           });

   connect(resetButton,
           &QAbstractButton::clicked,
           self_,
           [&buttonSettings]() { buttonSettings.StageDefaults(); });

   connections_.emplace_back(buttonSettings.staged_signal().connect(
      [resetButton, &buttonSettings]()
      { resetButton->setVisible(!buttonSettings.IsDefaultStaged()); }));
}

// NOLINTEND(cppcoreguidelines-owning-memory)

} // namespace scwx::qt::ui
