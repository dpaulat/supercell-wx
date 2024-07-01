#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/awips/phenomenon.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

class AudioSettings : public SettingsCategory
{
public:
   explicit AudioSettings();
   ~AudioSettings();

   AudioSettings(const AudioSettings&)            = delete;
   AudioSettings& operator=(const AudioSettings&) = delete;

   AudioSettings(AudioSettings&&) noexcept;
   AudioSettings& operator=(AudioSettings&&) noexcept;

   SettingsVariable<std::string>& alert_sound_file() const;
   SettingsVariable<std::string>& alert_location_method() const;
   SettingsVariable<double>&      alert_latitude() const;
   SettingsVariable<double>&      alert_longitude() const;
   SettingsVariable<double>&      alert_radius() const;
   SettingsVariable<std::string>& alert_county() const;
   SettingsVariable<bool>& alert_enabled(awips::Phenomenon phenomenon) const;
   SettingsVariable<bool>& ignore_missing_codecs() const;

   static AudioSettings& Instance();

   friend bool operator==(const AudioSettings& lhs, const AudioSettings& rhs);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
