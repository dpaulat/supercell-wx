#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/settings_container.hpp>
#include <scwx/qt/settings/settings_definitions.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/types/alert_types.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/types/time_types.hpp>
#include <scwx/util/time.hpp>

#include <array>

#include <boost/algorithm/string.hpp>
#include <QUrl>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::general_settings";

class GeneralSettings::Impl
{
public:
   explicit Impl()
   {
      const std::string defaultWarningsProviderValue =
         "https://warnings.allisonhouse.com";

      std::string defaultClockFormatValue =
         scwx::util::GetClockFormatName(scwx::util::ClockFormat::_24Hour);
      std::string defaultDefaultAlertActionValue =
         types::GetAlertActionName(types::AlertAction::Go);
      std::string defaultDefaultTimeZoneValue =
         types::GetDefaultTimeZoneName(types::DefaultTimeZone::Radar);
      std::string defaultMapProviderValue =
         map::GetMapProviderName(map::MapProvider::MapTiler);
      std::string defaultThemeValue =
         types::GetUiStyleName(types::UiStyle::Default);

      boost::to_lower(defaultClockFormatValue);
      boost::to_lower(defaultDefaultAlertActionValue);
      boost::to_lower(defaultDefaultTimeZoneValue);
      boost::to_lower(defaultMapProviderValue);
      boost::to_lower(defaultThemeValue);

      antiAliasingEnabled_.SetDefault(true);
      clockFormat_.SetDefault(defaultClockFormatValue);
      debugEnabled_.SetDefault(false);
      defaultAlertAction_.SetDefault(defaultDefaultAlertActionValue);
      defaultRadarSite_.SetDefault("KLSX");
      defaultTimeZone_.SetDefault(defaultDefaultTimeZoneValue);
      fontSizes_.SetDefault({16});
      loopDelay_.SetDefault(2500);
      loopSpeed_.SetDefault(5.0);
      loopTime_.SetDefault(30);
      gridWidth_.SetDefault(1);
      gridHeight_.SetDefault(1);
      mapProvider_.SetDefault(defaultMapProviderValue);
      mapboxApiKey_.SetDefault("?");
      maptilerApiKey_.SetDefault("?");
      showMapAttribution_.SetDefault(true);
      showMapCenter_.SetDefault(false);
      showMapLogo_.SetDefault(true);
      theme_.SetDefault(defaultThemeValue);
      trackLocation_.SetDefault(false);
      updateNotificationsEnabled_.SetDefault(true);
      warningsProvider_.SetDefault(defaultWarningsProviderValue);

      fontSizes_.SetElementMinimum(1);
      fontSizes_.SetElementMaximum(72);
      fontSizes_.SetValidator([](const std::vector<std::int64_t>& value)
                              { return !value.empty(); });
      gridWidth_.SetMinimum(1);
      gridWidth_.SetMaximum(2);
      gridHeight_.SetMinimum(1);
      gridHeight_.SetMaximum(2);
      loopDelay_.SetMinimum(0);
      loopDelay_.SetMaximum(15000);
      loopSpeed_.SetMinimum(1.0);
      loopSpeed_.SetMaximum(99.99);
      loopTime_.SetMinimum(1);
      loopTime_.SetMaximum(1440);

      clockFormat_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(scwx::util::ClockFormat,
                                      scwx::util::ClockFormatIterator(),
                                      scwx::util::GetClockFormatName));
      defaultAlertAction_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::AlertAction,
                                      types::AlertActionIterator(),
                                      types::GetAlertActionName));
      defaultTimeZone_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::DefaultTimeZone,
                                      types::DefaultTimeZoneIterator(),
                                      types::GetDefaultTimeZoneName));
      mapProvider_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(map::MapProvider,
                                      map::MapProviderIterator(),
                                      map::GetMapProviderName));
      mapboxApiKey_.SetValidator([](const std::string& value)
                                 { return !value.empty(); });
      maptilerApiKey_.SetValidator([](const std::string& value)
                                   { return !value.empty(); });
      theme_.SetValidator(                            //
         SCWX_SETTINGS_ENUM_VALIDATOR(types::UiStyle, //
                                      types::UiStyleIterator(),
                                      types::GetUiStyleName));
      warningsProvider_.SetValidator(
         [](const std::string& value)
         { return QUrl {QString::fromStdString(value)}.isValid(); });
   }

   ~Impl() {}

   SettingsVariable<bool>        antiAliasingEnabled_ {"anti_aliasing_enabled"};
   SettingsVariable<std::string> clockFormat_ {"clock_format"};
   SettingsVariable<bool>        debugEnabled_ {"debug_enabled"};
   SettingsVariable<std::string> defaultAlertAction_ {"default_alert_action"};
   SettingsVariable<std::string> defaultRadarSite_ {"default_radar_site"};
   SettingsVariable<std::string> defaultTimeZone_ {"default_time_zone"};
   SettingsContainer<std::vector<std::int64_t>> fontSizes_ {"font_sizes"};
   SettingsVariable<std::int64_t>               gridWidth_ {"grid_width"};
   SettingsVariable<std::int64_t>               gridHeight_ {"grid_height"};
   SettingsVariable<std::int64_t>               loopDelay_ {"loop_delay"};
   SettingsVariable<double>                     loopSpeed_ {"loop_speed"};
   SettingsVariable<std::int64_t>               loopTime_ {"loop_time"};
   SettingsVariable<std::string>                mapProvider_ {"map_provider"};
   SettingsVariable<std::string> mapboxApiKey_ {"mapbox_api_key"};
   SettingsVariable<std::string> maptilerApiKey_ {"maptiler_api_key"};
   SettingsVariable<bool>        showMapAttribution_ {"show_map_attribution"};
   SettingsVariable<bool>        showMapCenter_ {"show_map_center"};
   SettingsVariable<bool>        showMapLogo_ {"show_map_logo"};
   SettingsVariable<std::string> theme_ {"theme"};
   SettingsVariable<bool>        trackLocation_ {"track_location"};
   SettingsVariable<bool> updateNotificationsEnabled_ {"update_notifications"};
   SettingsVariable<std::string> warningsProvider_ {"warnings_provider"};
};

GeneralSettings::GeneralSettings() :
    SettingsCategory("general"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->antiAliasingEnabled_, //
                      &p->clockFormat_,
                      &p->debugEnabled_,
                      &p->defaultAlertAction_,
                      &p->defaultRadarSite_,
                      &p->defaultTimeZone_,
                      &p->fontSizes_,
                      &p->gridWidth_,
                      &p->gridHeight_,
                      &p->loopDelay_,
                      &p->loopSpeed_,
                      &p->loopTime_,
                      &p->mapProvider_,
                      &p->mapboxApiKey_,
                      &p->maptilerApiKey_,
                      &p->showMapAttribution_,
                      &p->showMapCenter_,
                      &p->showMapLogo_,
                      &p->theme_,
                      &p->trackLocation_,
                      &p->updateNotificationsEnabled_,
                      &p->warningsProvider_});
   SetDefaults();
}
GeneralSettings::~GeneralSettings() = default;

GeneralSettings::GeneralSettings(GeneralSettings&&) noexcept = default;
GeneralSettings&
GeneralSettings::operator=(GeneralSettings&&) noexcept = default;

SettingsVariable<bool>& GeneralSettings::anti_aliasing_enabled() const
{
   return p->antiAliasingEnabled_;
}

SettingsVariable<std::string>& GeneralSettings::clock_format() const
{
   return p->clockFormat_;
}

SettingsVariable<bool>& GeneralSettings::debug_enabled() const
{
   return p->debugEnabled_;
}

SettingsVariable<std::string>& GeneralSettings::default_alert_action() const
{
   return p->defaultAlertAction_;
}

SettingsVariable<std::string>& GeneralSettings::default_radar_site() const
{
   return p->defaultRadarSite_;
}

SettingsVariable<std::string>& GeneralSettings::default_time_zone() const
{
   return p->defaultTimeZone_;
}

SettingsContainer<std::vector<std::int64_t>>&
GeneralSettings::font_sizes() const
{
   return p->fontSizes_;
}

SettingsVariable<std::int64_t>& GeneralSettings::grid_height() const
{
   return p->gridHeight_;
}

SettingsVariable<std::int64_t>& GeneralSettings::grid_width() const
{
   return p->gridWidth_;
}

SettingsVariable<std::int64_t>& GeneralSettings::loop_delay() const
{
   return p->loopDelay_;
}

SettingsVariable<double>& GeneralSettings::loop_speed() const
{
   return p->loopSpeed_;
}

SettingsVariable<std::int64_t>& GeneralSettings::loop_time() const
{
   return p->loopTime_;
}

SettingsVariable<std::string>& GeneralSettings::map_provider() const
{
   return p->mapProvider_;
}

SettingsVariable<std::string>& GeneralSettings::mapbox_api_key() const
{
   return p->mapboxApiKey_;
}

SettingsVariable<std::string>& GeneralSettings::maptiler_api_key() const
{
   return p->maptilerApiKey_;
}

SettingsVariable<bool>& GeneralSettings::show_map_attribution() const
{
   return p->showMapAttribution_;
}

SettingsVariable<bool>& GeneralSettings::show_map_center() const
{
   return p->showMapCenter_;
}

SettingsVariable<bool>& GeneralSettings::show_map_logo() const
{
   return p->showMapLogo_;
}

SettingsVariable<std::string>& GeneralSettings::theme() const
{
   return p->theme_;
}

SettingsVariable<bool>& GeneralSettings::track_location() const
{
   return p->trackLocation_;
}

SettingsVariable<bool>& GeneralSettings::update_notifications_enabled() const
{
   return p->updateNotificationsEnabled_;
}

SettingsVariable<std::string>& GeneralSettings::warnings_provider() const
{
   return p->warningsProvider_;
}

bool GeneralSettings::Shutdown()
{
   bool dataChanged = false;

   // Commit settings that are managed separate from the settings dialog
   dataChanged |= p->loopDelay_.Commit();
   dataChanged |= p->loopSpeed_.Commit();
   dataChanged |= p->loopTime_.Commit();
   dataChanged |= p->trackLocation_.Commit();

   return dataChanged;
}

GeneralSettings& GeneralSettings::Instance()
{
   static GeneralSettings generalSettings_;
   return generalSettings_;
}

bool operator==(const GeneralSettings& lhs, const GeneralSettings& rhs)
{
   return (lhs.p->antiAliasingEnabled_ == rhs.p->antiAliasingEnabled_ &&
           lhs.p->clockFormat_ == rhs.p->clockFormat_ &&
           lhs.p->debugEnabled_ == rhs.p->debugEnabled_ &&
           lhs.p->defaultAlertAction_ == rhs.p->defaultAlertAction_ &&
           lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_ &&
           lhs.p->defaultTimeZone_ == rhs.p->defaultTimeZone_ &&
           lhs.p->fontSizes_ == rhs.p->fontSizes_ &&
           lhs.p->gridWidth_ == rhs.p->gridWidth_ &&
           lhs.p->gridHeight_ == rhs.p->gridHeight_ &&
           lhs.p->loopDelay_ == rhs.p->loopDelay_ &&
           lhs.p->loopSpeed_ == rhs.p->loopSpeed_ &&
           lhs.p->loopTime_ == rhs.p->loopTime_ &&
           lhs.p->mapProvider_ == rhs.p->mapProvider_ &&
           lhs.p->mapboxApiKey_ == rhs.p->mapboxApiKey_ &&
           lhs.p->maptilerApiKey_ == rhs.p->maptilerApiKey_ &&
           lhs.p->showMapAttribution_ == rhs.p->showMapAttribution_ &&
           lhs.p->showMapCenter_ == rhs.p->showMapCenter_ &&
           lhs.p->showMapLogo_ == rhs.p->showMapLogo_ &&
           lhs.p->theme_ == rhs.p->theme_ &&
           lhs.p->trackLocation_ == rhs.p->trackLocation_ &&
           lhs.p->updateNotificationsEnabled_ ==
              rhs.p->updateNotificationsEnabled_ &&
           lhs.p->warningsProvider_ == rhs.p->warningsProvider_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
