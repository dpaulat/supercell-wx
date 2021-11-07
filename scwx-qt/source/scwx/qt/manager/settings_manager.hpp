#pragma once

#include <scwx/qt/settings/general_settings.hpp>
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

std::shared_ptr<settings::GeneralSettings> general_settings();
std::shared_ptr<settings::PaletteSettings> palette_settings();

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
