#include <scwx/qt/settings/ui_settings.hpp>

namespace scwx::qt::settings
{

static const std::string logPrefix_ = "scwx::qt::settings::ui_settings";

class UiSettingsImpl
{
public:
   explicit UiSettingsImpl()
   {
      // SetDefault, SetMinimum and SetMaximum are descriptive
      // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
      level2ProductsExpanded_.SetDefault(false);
      level2SettingsExpanded_.SetDefault(true);
      level3ProductsExpanded_.SetDefault(true);
      level3SettingsExpanded_.SetDefault(true);
      mapSettingsExpanded_.SetDefault(true);
      timelineExpanded_.SetDefault(true);
      spcOutlookExpanded_.SetDefault(false);
      mainUIState_.SetDefault("");
      mainUIGeometry_.SetDefault("");
      radarToolboxDockWidth_.SetDefault(280);
      radarToolboxDockWidth_.SetMinimum(150);
      radarToolboxDockWidth_.SetMaximum(600);
      // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
   }

   ~UiSettingsImpl()                                 = default;
   UiSettingsImpl(const UiSettingsImpl&)             = delete;
   UiSettingsImpl& operator=(const UiSettingsImpl&)  = delete;
   UiSettingsImpl(const UiSettingsImpl&&)            = delete;
   UiSettingsImpl& operator=(const UiSettingsImpl&&) = delete;

   SettingsVariable<bool> level2ProductsExpanded_ {"level2_products_expanded"};
   SettingsVariable<bool> level2SettingsExpanded_ {"level2_settings_expanded"};
   SettingsVariable<bool> level3ProductsExpanded_ {"level3_products_expanded"};
   SettingsVariable<bool> level3SettingsExpanded_ {"level3_settings_expanded"};
   SettingsVariable<bool> mapSettingsExpanded_ {"map_settings_expanded"};
   SettingsVariable<bool> timelineExpanded_ {"timeline_expanded"};
   SettingsVariable<bool> spcOutlookExpanded_ {"spc_outlook_expanded"};
   SettingsVariable<std::string>  mainUIState_ {"main_ui_state"};
   SettingsVariable<std::string>  mainUIGeometry_ {"main_ui_geometry"};
   SettingsVariable<std::int64_t> radarToolboxDockWidth_ {
      "radar_toolbox_dock_width"};
};

UiSettings::UiSettings() :
    SettingsCategory("ui"), p(std::make_unique<UiSettingsImpl>())
{
   RegisterVariables({&p->level2ProductsExpanded_,
                      &p->level2SettingsExpanded_,
                      &p->level3ProductsExpanded_,
                      &p->level3SettingsExpanded_,
                      &p->mapSettingsExpanded_,
                      &p->timelineExpanded_,
                      &p->spcOutlookExpanded_,
                      &p->mainUIState_,
                      &p->mainUIGeometry_,
                      &p->radarToolboxDockWidth_});
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

SettingsVariable<bool>& UiSettings::level3_settings_expanded() const
{
   return p->level3SettingsExpanded_;
}

SettingsVariable<bool>& UiSettings::map_settings_expanded() const
{
   return p->mapSettingsExpanded_;
}

SettingsVariable<bool>& UiSettings::timeline_expanded() const
{
   return p->timelineExpanded_;
}

SettingsVariable<bool>& UiSettings::spc_outlook_expanded() const
{
   return p->spcOutlookExpanded_;
}

SettingsVariable<std::string>& UiSettings::main_ui_state() const
{
   return p->mainUIState_;
}

SettingsVariable<std::string>& UiSettings::main_ui_geometry() const
{
   return p->mainUIGeometry_;
}

SettingsVariable<std::int64_t>& UiSettings::radar_toolbox_dock_width() const
{
   return p->radarToolboxDockWidth_;
}

bool UiSettings::Shutdown()
{
   bool dataChanged = false;

   // Commit settings that are managed separate from the settings dialog
   dataChanged |= p->level2ProductsExpanded_.Commit();
   dataChanged |= p->level2SettingsExpanded_.Commit();
   dataChanged |= p->level3ProductsExpanded_.Commit();
   dataChanged |= p->level3SettingsExpanded_.Commit();
   dataChanged |= p->mapSettingsExpanded_.Commit();
   dataChanged |= p->timelineExpanded_.Commit();
   dataChanged |= p->spcOutlookExpanded_.Commit();
   dataChanged |= p->mainUIState_.Commit();
   dataChanged |= p->mainUIGeometry_.Commit();
   dataChanged |= p->radarToolboxDockWidth_.Commit();

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
           lhs.p->level3SettingsExpanded_ == rhs.p->level3SettingsExpanded_ &&
           lhs.p->mapSettingsExpanded_ == rhs.p->mapSettingsExpanded_ &&
           lhs.p->timelineExpanded_ == rhs.p->timelineExpanded_ &&
           lhs.p->mainUIState_ == rhs.p->mainUIState_ &&
           lhs.p->mainUIGeometry_ == rhs.p->mainUIGeometry_ &&
           lhs.p->radarToolboxDockWidth_ == rhs.p->radarToolboxDockWidth_);
}

} // namespace scwx::qt::settings
