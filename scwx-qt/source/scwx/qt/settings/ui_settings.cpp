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
      mainUIState_.SetDefault("");
      mainUIGeometry_.SetDefault("");
      mapAnnotationState_.SetDefault("");
      mapPaneSplitterState_.SetDefault("");
      mapPanePopoutState_.SetDefault("");
      mapPaneViewLinkState_.SetDefault("");
      panesMatchMapStyle_.SetDefault(true);
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
   SettingsVariable<std::string> mainUIState_ {"main_ui_state"};
   SettingsVariable<std::string> mainUIGeometry_ {"main_ui_geometry"};
   SettingsVariable<std::string> mapAnnotationState_ {"map_annotation_state"};
   SettingsVariable<std::string> mapPaneSplitterState_ {
      "map_pane_splitter_state"};
   SettingsVariable<std::string> mapPanePopoutState_ {"map_pane_popout_state"};
   SettingsVariable<std::string> mapPaneViewLinkState_ {
      "map_pane_view_link_state"};
   SettingsVariable<bool> panesMatchMapStyle_ {"panes_match_map_style"};
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
                      &p->mainUIState_,
                      &p->mainUIGeometry_,
                      &p->mapAnnotationState_,
                      &p->mapPaneSplitterState_,
                      &p->mapPanePopoutState_,
                      &p->mapPaneViewLinkState_,
                      &p->panesMatchMapStyle_});
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

SettingsVariable<std::string>& UiSettings::main_ui_state() const
{
   return p->mainUIState_;
}

SettingsVariable<std::string>& UiSettings::main_ui_geometry() const
{
   return p->mainUIGeometry_;
}

SettingsVariable<std::string>& UiSettings::map_annotation_state() const
{
   return p->mapAnnotationState_;
}

SettingsVariable<std::string>& UiSettings::map_pane_splitter_state() const
{
   return p->mapPaneSplitterState_;
}

SettingsVariable<std::string>& UiSettings::map_pane_popout_state() const
{
   return p->mapPanePopoutState_;
}

SettingsVariable<std::string>& UiSettings::map_pane_view_link_state() const
{
   return p->mapPaneViewLinkState_;
}

SettingsVariable<bool>& UiSettings::panes_match_map_style() const
{
   return p->panesMatchMapStyle_;
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
   dataChanged |= p->mainUIState_.Commit();
   dataChanged |= p->mainUIGeometry_.Commit();
   dataChanged |= p->mapAnnotationState_.Commit();
   dataChanged |= p->mapPaneSplitterState_.Commit();
   dataChanged |= p->mapPanePopoutState_.Commit();
   dataChanged |= p->mapPaneViewLinkState_.Commit();
   dataChanged |= p->panesMatchMapStyle_.Commit();

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
           lhs.p->mapAnnotationState_ == rhs.p->mapAnnotationState_ &&
           lhs.p->mapPaneSplitterState_ == rhs.p->mapPaneSplitterState_ &&
           lhs.p->mapPanePopoutState_ == rhs.p->mapPanePopoutState_ &&
           lhs.p->mapPaneViewLinkState_ == rhs.p->mapPaneViewLinkState_ &&
           lhs.p->panesMatchMapStyle_ == rhs.p->panesMatchMapStyle_);
}

} // namespace scwx::qt::settings
