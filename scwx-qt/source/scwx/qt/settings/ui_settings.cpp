#include <scwx/qt/settings/ui_settings.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::ui_settings";

class UiSettingsImpl
{
public:
   explicit UiSettingsImpl()
   {
      level2ProductsExpanded_.SetDefault(false);
      level2SettingsExpanded_.SetDefault(true);
      level3ProductsExpanded_.SetDefault(true);
      mapSettingsExpanded_.SetDefault(true);
      timelineExpanded_.SetDefault(true);
      radarToolboxDockVisible_.SetDefault(true);
      mainUIState_.SetDefault("");
   }

   ~UiSettingsImpl() {}

   SettingsVariable<bool> level2ProductsExpanded_ {"level2_products_expanded"};
   SettingsVariable<bool> level2SettingsExpanded_ {"level2_settings_expanded"};
   SettingsVariable<bool> level3ProductsExpanded_ {"level3_products_expanded"};
   SettingsVariable<bool> mapSettingsExpanded_ {"map_settings_expanded"};
   SettingsVariable<bool> timelineExpanded_ {"timeline_expanded"};
   SettingsVariable<bool> radarToolboxDockVisible_ {"radar_toolbox_dock_visible"};
   SettingsVariable<std::string> mainUIState_ {"main_ui_state"};
};

UiSettings::UiSettings() :
    SettingsCategory("ui"), p(std::make_unique<UiSettingsImpl>())
{
   RegisterVariables({&p->level2ProductsExpanded_,
                      &p->level2SettingsExpanded_,
                      &p->level3ProductsExpanded_,
                      &p->mapSettingsExpanded_,
                      &p->timelineExpanded_,
                      &p->radarToolboxDockVisible_,
                      &p->mainUIState_});
   SetDefaults();
}
UiSettings::~UiSettings() = default;

UiSettings::UiSettings(UiSettings&&) noexcept            = default;
UiSettings& UiSettings::operator=(UiSettings&&) noexcept = default;

SettingsVariable<bool>& UiSettings::level2_products_expanded() const
{
   return p->level2ProductsExpanded_;
}

SettingsVariable<bool>& UiSettings::level2_settings_expanded() const
{
   return p->level2SettingsExpanded_;
}

SettingsVariable<bool>& UiSettings::level3_products_expanded() const
{
   return p->level3ProductsExpanded_;
}

SettingsVariable<bool>& UiSettings::map_settings_expanded() const
{
   return p->mapSettingsExpanded_;
}

SettingsVariable<bool>& UiSettings::timeline_expanded() const
{
   return p->timelineExpanded_;
}


SettingsVariable<bool>& UiSettings::radar_toolbox_dock_visible() const
{
   return p->radarToolboxDockVisible_;
}

SettingsVariable<std::string>& UiSettings::main_ui_state() const
{
   return p->mainUIState_;
}


bool UiSettings::Shutdown()
{
   bool dataChanged = false;

   // Commit settings that are managed separate from the settings dialog
   dataChanged |= p->level2ProductsExpanded_.Commit();
   dataChanged |= p->level2SettingsExpanded_.Commit();
   dataChanged |= p->level3ProductsExpanded_.Commit();
   dataChanged |= p->mapSettingsExpanded_.Commit();
   dataChanged |= p->timelineExpanded_.Commit();
   dataChanged |= p->radarToolboxDockVisible_.Commit();
   dataChanged |= p->mainUIState_.Commit();

   return dataChanged;
}

UiSettings& UiSettings::Instance()
{
   static UiSettings uiSettings_;
   return uiSettings_;
}

bool operator==(const UiSettings& lhs, const UiSettings& rhs)
{
   return (lhs.p->level2ProductsExpanded_ == rhs.p->level2ProductsExpanded_ &&
           lhs.p->level2SettingsExpanded_ == rhs.p->level2SettingsExpanded_ &&
           lhs.p->level3ProductsExpanded_ == rhs.p->level3ProductsExpanded_ &&
           lhs.p->mapSettingsExpanded_ == rhs.p->mapSettingsExpanded_ &&
           lhs.p->timelineExpanded_ == rhs.p->timelineExpanded_ &&
           lhs.p->radarToolboxDockVisible_ == rhs.p->radarToolboxDockVisible_ &&
           lhs.p->mainUIState_ == rhs.p->mainUIState_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
