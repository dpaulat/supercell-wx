#pragma once

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class AlertManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(AlertManager)

public:
   explicit AlertManager();
   ~AlertManager();

   void AlertManager::SetRadarSite(const std::string& radarSite);
   static std::shared_ptr<AlertManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
