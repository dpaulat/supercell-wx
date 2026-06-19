#pragma once

#include <scwx/qt/types/radar_site_types.hpp>

#include <memory>

#include <QObject>

namespace scwx::qt::manager
{

/**
 * @brief Radar Site Status Manager
 */
class RadarSiteStatusManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(RadarSiteStatusManager)

public:
   explicit RadarSiteStatusManager();
   ~RadarSiteStatusManager() override;

   void Start();
   void Stop();

   static std::shared_ptr<RadarSiteStatusManager> Instance();

signals:
   void StatusUpdated();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
