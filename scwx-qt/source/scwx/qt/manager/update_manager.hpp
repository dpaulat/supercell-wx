#pragma once

#include <scwx/qt/types/github_types.hpp>

#include <memory>
#include <string>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class UpdateManager : public QObject
{
   Q_OBJECT

public:
   explicit UpdateManager();
   ~UpdateManager();

   types::gh::Release latest_release() const;
   std::string        latest_version() const;

   bool CheckForUpdates(const std::string& currentVersion = {});

   static std::shared_ptr<UpdateManager> Instance();

signals:
   void UpdateAvailable(const std::string&        latestVersion,
                        const types::gh::Release& latestRelease);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
