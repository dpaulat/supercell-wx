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

   void TrackLocation(boost::uuids::uuid uuid, bool trackingEnabled);

   static std::shared_ptr<PositionManager> Instance();

signals:
   void PositionUpdated(const QGeoPositionInfo& info);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
