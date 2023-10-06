#pragma once

#include <string>

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
void SaveSettings();
void Shutdown();

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
