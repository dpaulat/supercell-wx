#pragma once

#include <memory>

#include <boost/uuid/uuid.hpp>
#include <QObject>

class QGeoPositionInfo;

namespace scwx
{
namespace qt
{
namespace manager
{

class PositionManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(PositionManager)

public:
   explicit PositionManager();
   ~PositionManager();

   QGeoPositionInfo position() const;

   bool IsLocationTracked();

   void EnablePositionUpdates(boost::uuids::uuid uuid, bool enabled);
   void TrackLocation(bool trackingEnabled);

   static std::shared_ptr<PositionManager> Instance();

signals:
   void LocationTrackingChanged(bool trackingEnabled);
   void PositionUpdated(const QGeoPositionInfo& info);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
