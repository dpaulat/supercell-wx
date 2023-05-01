#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/settings_container.hpp>
#include <scwx/qt/map/map_provider.hpp>

#include <array>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::general_settings";

class GeneralSettingsImpl
{
public:
   explicit GeneralSettingsImpl()
   {
      debugEnabled_.SetDefault(false);
      defaultRadarSite_.SetDefault("KLSX");
      fontSizes_.SetDefault({16});
      gridWidth_.SetDefault(1);
      gridHeight_.SetDefault(1);
      mapProvider_.SetDefault("maptiler");
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

   ~GeneralSettingsImpl() {}

   SettingsVariable<bool>        debugEnabled_ {"debug_enabled"};
   SettingsVariable<std::string> defaultRadarSite_ {"default_radar_site"};
   SettingsContainer<std::vector<std::int64_t>> fontSizes_ {"font_sizes"};
   SettingsVariable<std::int64_t>               gridWidth_ {"grid_width"};
   SettingsVariable<std::int64_t>               gridHeight_ {"grid_height"};
   SettingsVariable<std::string>                mapProvider_ {"map_provider"};
   SettingsVariable<std::string> mapboxApiKey_ {"mapbox_api_key"};
   SettingsVariable<std::string> maptilerApiKey_ {"maptiler_api_key"};
   SettingsVariable<bool> updateNotificationsEnabled_ {"update_notifications"};
};

GeneralSettings::GeneralSettings() :
    SettingsCategory("general"), p(std::make_unique<GeneralSettingsImpl>())
{
   RegisterVariables({&p->debugEnabled_,
                      &p->defaultRadarSite_,
                      &p->fontSizes_,
                      &p->gridWidth_,
                      &p->gridHeight_,
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

bool operator==(const GeneralSettings& lhs, const GeneralSettings& rhs)
{
   return (lhs.p->debugEnabled_ == rhs.p->debugEnabled_ &&
           lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_ &&
           lhs.p->fontSizes_ == rhs.p->fontSizes_ &&
           lhs.p->gridWidth_ == rhs.p->gridWidth_ &&
           lhs.p->gridHeight_ == rhs.p->gridHeight_ &&
           lhs.p->mapProvider_ == rhs.p->mapProvider_ &&
           lhs.p->mapboxApiKey_ == rhs.p->mapboxApiKey_ &&
           lhs.p->maptilerApiKey_ == rhs.p->maptilerApiKey_ &&
           lhs.p->updateNotificationsEnabled_ ==
              rhs.p->updateNotificationsEnabled_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
