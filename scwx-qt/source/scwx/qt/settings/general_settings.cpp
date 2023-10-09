#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/settings_container.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/types/alert_types.hpp>

#include <array>

#include <boost/algorithm/string.hpp>

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
      std::string defaultDefaultAlertActionValue =
         types::GetAlertActionName(types::AlertAction::Go);
      std::string defaultMapProviderValue =
         map::GetMapProviderName(map::MapProvider::MapTiler);

      boost::to_lower(defaultDefaultAlertActionValue);
      boost::to_lower(defaultMapProviderValue);

      debugEnabled_.SetDefault(false);
      defaultAlertAction_.SetDefault(defaultDefaultAlertActionValue);
      defaultRadarSite_.SetDefault("KLSX");
      fontSizes_.SetDefault({16});
      loopDelay_.SetDefault(2500);
      loopSpeed_.SetDefault(5.0);
      loopTime_.SetDefault(30);
      gridWidth_.SetDefault(1);
      gridHeight_.SetDefault(1);
      mapProvider_.SetDefault(defaultMapProviderValue);
      mapboxApiKey_.SetDefault("?");
      maptilerApiKey_.SetDefault("?");
      updateNotificationsEnabled_.SetDefault(true);

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

      defaultAlertAction_.SetValidator(
         [](const std::string& value)
         {
            for (types::AlertAction alertAction : types::AlertActionIterator())
            {
               // If the value is equal to a lower case alert action name
               std::string alertActionName =
                  types::GetAlertActionName(alertAction);
               boost::to_lower(alertActionName);
               if (value == alertActionName)
               {
                  // Regard as a match, valid
                  return true;
               }
            }

            // No match found, invalid
            return false;
         });
      mapProvider_.SetValidator(
         [](const std::string& value)
         {
            for (map::MapProvider mapProvider : map::MapProviderIterator())
            {
               // If the value is equal to a lower case map provider name
               std::string mapProviderName =
                  map::GetMapProviderName(mapProvider);
               boost::to_lower(mapProviderName);
               if (value == mapProviderName)
               {
                  // Regard as a match, valid
                  return true;
               }
            }

            // No match found, invalid
            return false;
         });
      mapboxApiKey_.SetValidator([](const std::string& value)
                                 { return !value.empty(); });
      maptilerApiKey_.SetValidator([](const std::string& value)
                                   { return !value.empty(); });
   }

   ~Impl() {}

   SettingsVariable<bool>        debugEnabled_ {"debug_enabled"};
   SettingsVariable<std::string> defaultAlertAction_ {"default_alert_action"};
   SettingsVariable<std::string> defaultRadarSite_ {"default_radar_site"};
   SettingsContainer<std::vector<std::int64_t>> fontSizes_ {"font_sizes"};
   SettingsVariable<std::int64_t>               gridWidth_ {"grid_width"};
   SettingsVariable<std::int64_t>               gridHeight_ {"grid_height"};
   SettingsVariable<std::int64_t>               loopDelay_ {"loop_delay"};
   SettingsVariable<double>                     loopSpeed_ {"loop_speed"};
   SettingsVariable<std::int64_t>               loopTime_ {"loop_time"};
   SettingsVariable<std::string>                mapProvider_ {"map_provider"};
   SettingsVariable<std::string> mapboxApiKey_ {"mapbox_api_key"};
   SettingsVariable<std::string> maptilerApiKey_ {"maptiler_api_key"};
   SettingsVariable<bool> updateNotificationsEnabled_ {"update_notifications"};
};

GeneralSettings::GeneralSettings() :
    SettingsCategory("general"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->debugEnabled_,
                      &p->defaultAlertAction_,
                      &p->defaultRadarSite_,
                      &p->fontSizes_,
                      &p->gridWidth_,
                      &p->gridHeight_,
                      &p->loopDelay_,
                      &p->loopSpeed_,
                      &p->loopTime_,
                      &p->mapProvider_,
                      &p->mapboxApiKey_,
                      &p->maptilerApiKey_,
                      &p->updateNotificationsEnabled_});
   SetDefaults();
}
GeneralSettings::~GeneralSettings() = default;

GeneralSettings::GeneralSettings(GeneralSettings&&) noexcept = default;
GeneralSettings&
GeneralSettings::operator=(GeneralSettings&&) noexcept = default;

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

SettingsVariable<bool>& GeneralSettings::update_notifications_enabled() const
{
   return p->updateNotificationsEnabled_;
}

bool GeneralSettings::Shutdown()
{
   bool dataChanged = false;

   // Commit settings that are managed separate from the settings dialog
   dataChanged |= p->loopDelay_.Commit();
   dataChanged |= p->loopSpeed_.Commit();
   dataChanged |= p->loopTime_.Commit();

   return dataChanged;
}

GeneralSettings& GeneralSettings::Instance()
{
   static GeneralSettings generalSettings_;
   return generalSettings_;
}

bool operator==(const GeneralSettings& lhs, const GeneralSettings& rhs)
{
   return (lhs.p->debugEnabled_ == rhs.p->debugEnabled_ &&
           lhs.p->defaultAlertAction_ == rhs.p->defaultAlertAction_ &&
           lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_ &&
           lhs.p->fontSizes_ == rhs.p->fontSizes_ &&
           lhs.p->gridWidth_ == rhs.p->gridWidth_ &&
           lhs.p->gridHeight_ == rhs.p->gridHeight_ &&
           lhs.p->loopDelay_ == rhs.p->loopDelay_ &&
           lhs.p->loopSpeed_ == rhs.p->loopSpeed_ &&
           lhs.p->loopTime_ == rhs.p->loopTime_ &&
           lhs.p->mapProvider_ == rhs.p->mapProvider_ &&
           lhs.p->mapboxApiKey_ == rhs.p->mapboxApiKey_ &&
           lhs.p->maptilerApiKey_ == rhs.p->maptilerApiKey_ &&
           lhs.p->updateNotificationsEnabled_ ==
              rhs.p->updateNotificationsEnabled_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
