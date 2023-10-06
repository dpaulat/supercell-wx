#pragma once

#include <string>
#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class SettingsManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(SettingsManager)

public:
   explicit SettingsManager();
   ~SettingsManager();

   void Initialize();
   void ReadSettings(const std::string& settingsPath);
   void SaveSettings();
   void Shutdown();

   static SettingsManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
