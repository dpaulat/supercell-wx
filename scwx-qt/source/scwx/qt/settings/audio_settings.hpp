#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/awips/phenomenon.hpp>

#include <memory>
#include <string>

namespace scwx::qt::settings
{

class AudioSettings : public SettingsCategory
{
public:
   explicit AudioSettings();
   ~AudioSettings() override;

   AudioSettings(const AudioSettings&)            = delete;
   AudioSettings& operator=(const AudioSettings&) = delete;

   AudioSettings(AudioSettings&&) noexcept;
   AudioSettings& operator=(AudioSettings&&) noexcept;

   [[nodiscard]] SettingsVariable<std::string>& alert_sound_file() const;
   [[nodiscard]] SettingsVariable<std::string>& alert_location_method() const;
   [[nodiscard]] SettingsVariable<double>&      alert_latitude() const;
   [[nodiscard]] SettingsVariable<double>&      alert_longitude() const;
   [[nodiscard]] SettingsVariable<double>&      alert_radius() const;
   [[nodiscard]] SettingsVariable<std::string>& alert_radar_site() const;
   [[nodiscard]] SettingsVariable<std::string>& alert_county() const;
   [[nodiscard]] SettingsVariable<std::string>& alert_wfo() const;
   [[nodiscard]] SettingsVariable<bool>&
   alert_enabled(awips::Phenomenon phenomenon) const;
   [[nodiscard]] SettingsVariable<bool>&         ignore_missing_codecs() const;
   [[nodiscard]] SettingsVariable<std::int64_t>& master_volume() const;

   static AudioSettings& Instance();

   friend bool operator==(const AudioSettings& lhs, const AudioSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
