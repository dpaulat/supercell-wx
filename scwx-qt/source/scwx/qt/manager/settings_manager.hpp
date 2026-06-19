#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <string>

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
   void ReadSettings(std::istream& is);
   void SaveSettings();
   void WriteSettings(std::ostream& os);
   void Shutdown();

   static SettingsManager& Instance();

signals:
   void SettingsSaved();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
