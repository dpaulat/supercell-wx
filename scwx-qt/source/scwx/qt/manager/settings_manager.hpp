#pragma once

#include <scwx/qt/settings/general_settings.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace SettingsManager
{

bool Initialize();
void ReadSettings(const std::string& settingsPath);

std::shared_ptr<settings::GeneralSettings> general_settings();

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
