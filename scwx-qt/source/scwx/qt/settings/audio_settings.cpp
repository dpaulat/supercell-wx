#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/settings/audio_settings.hpp>
#include <scwx/qt/settings/settings_definitions.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/types/alert_types.hpp>
#include <scwx/qt/types/location_types.hpp>
#include <scwx/qt/types/media_types.hpp>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::audio_settings";

static const bool              kDefaultAlertEnabled_ {false};
static const awips::Phenomenon kDefaultPhenomenon_ {awips::Phenomenon::Unknown};

class AudioSettings::Impl
{
public:
   explicit Impl()
   {
      std::string defaultAlertSoundFileValue =
         types::GetMediaPath(types::AudioFile::EasAttentionSignal);
      std::string defaultAlertLocationMethodValue =
         types::GetLocationMethodName(types::LocationMethod::Fixed);

      boost::to_lower(defaultAlertLocationMethodValue);

      alertSoundFile_.SetDefault(defaultAlertSoundFileValue);
      alertLocationMethod_.SetDefault(defaultAlertLocationMethodValue);
      alertLatitude_.SetDefault(0.0);
      alertLongitude_.SetDefault(0.0);
      alertRadius_.SetDefault(0.0);
      alertRadarSite_.SetDefault("default");
      ignoreMissingCodecs_.SetDefault(false);

      alertLatitude_.SetMinimum(-90.0);
      alertLatitude_.SetMaximum(90.0);
      alertLongitude_.SetMinimum(-180.0);
      alertLongitude_.SetMaximum(180.0);
      alertRadius_.SetMinimum(0.0);
      alertRadius_.SetMaximum(9999999999);


      alertLocationMethod_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::LocationMethod,
                                      types::LocationMethodIterator(),
                                      types::GetLocationMethodName));

      alertCounty_.SetValidator(
         [](const std::string& value)
         {
            // Empty, or county exists in the database
            return value.empty() ||
                   config::CountyDatabase::GetCountyName(value) != value;
         });

      auto& alertAudioPhenomena = types::GetAlertAudioPhenomena();
      alertEnabled_.reserve(alertAudioPhenomena.size() + 1);

      for (auto& phenomenon : alertAudioPhenomena)
      {
         std::string phenomenonCode = awips::GetPhenomenonCode(phenomenon);
         std::string name           = fmt::format("{}_enabled", phenomenonCode);

         auto result =
            alertEnabled_.emplace(phenomenon, SettingsVariable<bool> {name});

         SettingsVariable<bool>& variable = result.first->second;

         variable.SetDefault(kDefaultAlertEnabled_);

         variables_.push_back(&variable);
      }

      // Create a default disabled alert, not stored in the settings file
      alertEnabled_.emplace(kDefaultPhenomenon_,
                            SettingsVariable<bool> {"alert_disabled"});
   }

   ~Impl() {}

   SettingsVariable<std::string> alertSoundFile_ {"alert_sound_file"};
   SettingsVariable<std::string> alertLocationMethod_ {"alert_location_method"};
   SettingsVariable<double>      alertLatitude_ {"alert_latitude"};
   SettingsVariable<double>      alertLongitude_ {"alert_longitude"};
   SettingsVariable<std::string> alertRadarSite_ {"alert_radar_site"};
   SettingsVariable<double>      alertRadius_ {"alert_radius"};
   SettingsVariable<std::string> alertCounty_ {"alert_county"};
   SettingsVariable<bool>        ignoreMissingCodecs_ {"ignore_missing_codecs"};

   std::unordered_map<awips::Phenomenon, SettingsVariable<bool>>
                                      alertEnabled_ {};
   std::vector<SettingsVariableBase*> variables_ {};
};

AudioSettings::AudioSettings() :
    SettingsCategory("audio"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->alertSoundFile_,
                      &p->alertLocationMethod_,
                      &p->alertLatitude_,
                      &p->alertLongitude_,
                      &p->alertRadarSite_,
                      &p->alertRadius_,
                      &p->alertCounty_,
                      &p->ignoreMissingCodecs_});
   RegisterVariables(p->variables_);
   SetDefaults();

   p->variables_.clear();
}
AudioSettings::~AudioSettings() = default;

AudioSettings::AudioSettings(AudioSettings&&) noexcept            = default;
AudioSettings& AudioSettings::operator=(AudioSettings&&) noexcept = default;

SettingsVariable<std::string>& AudioSettings::alert_sound_file() const
{
   return p->alertSoundFile_;
}

SettingsVariable<std::string>& AudioSettings::alert_location_method() const
{
   return p->alertLocationMethod_;
}

SettingsVariable<double>& AudioSettings::alert_latitude() const
{
   return p->alertLatitude_;
}

SettingsVariable<double>& AudioSettings::alert_longitude() const
{
   return p->alertLongitude_;
}

SettingsVariable<std::string>& AudioSettings::alert_radar_site() const
{
   return p->alertRadarSite_;
}

SettingsVariable<double>& AudioSettings::alert_radius() const
{
   return p->alertRadius_;
}

SettingsVariable<std::string>& AudioSettings::alert_county() const
{
   return p->alertCounty_;
}

SettingsVariable<bool>&
AudioSettings::alert_enabled(awips::Phenomenon phenomenon) const
{
   auto alert = p->alertEnabled_.find(phenomenon);
   if (alert == p->alertEnabled_.cend())
   {
      alert = p->alertEnabled_.find(kDefaultPhenomenon_);
   }
   return alert->second;
}

SettingsVariable<bool>& AudioSettings::ignore_missing_codecs() const
{
   return p->ignoreMissingCodecs_;
}

AudioSettings& AudioSettings::Instance()
{
   static AudioSettings audioSettings_;
   return audioSettings_;
}

bool operator==(const AudioSettings& lhs, const AudioSettings& rhs)
{
   return (lhs.p->alertSoundFile_ == rhs.p->alertSoundFile_ &&
           lhs.p->alertLocationMethod_ == rhs.p->alertLocationMethod_ &&
           lhs.p->alertLatitude_ == rhs.p->alertLatitude_ &&
           lhs.p->alertLongitude_ == rhs.p->alertLongitude_ &&
           lhs.p->alertRadius_ == rhs.p->alertRadius_ &&
           lhs.p->alertCounty_ == rhs.p->alertCounty_ &&
           lhs.p->alertEnabled_ == rhs.p->alertEnabled_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
