#pragma once

#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace SettingsManager
{

void Initialize();
void ReadSettings(const std::string& settingsPath);

settings::GeneralSettings& general_settings();
settings::MapSettings&     map_settings();
settings::PaletteSettings& palette_settings();

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
