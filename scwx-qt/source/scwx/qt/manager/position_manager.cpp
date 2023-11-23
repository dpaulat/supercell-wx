#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <set>

#include <boost/uuid/random_generator.hpp>
#include <QGeoPositionInfoSource>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::position_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PositionManager::Impl
{
public:
   explicit Impl(PositionManager* self) :
       self_ {self}, trackingUuid_ {boost::uuids::random_generator()()}
   {
      // TODO: macOS requires permission
      geoPositionInfoSource_ =
         QGeoPositionInfoSource::createDefaultSource(self);

      if (geoPositionInfoSource_ != nullptr)
      {
         logger_->debug("Using position source: {}",
                        geoPositionInfoSource_->sourceName().toStdString());

         QObject::connect(geoPositionInfoSource_,
                          &QGeoPositionInfoSource::positionUpdated,
                          self_,
                          [this](const QGeoPositionInfo& info)
                          {
                             auto coordinate = info.coordinate();

                             if (coordinate != position_.coordinate())
                             {
                                logger_->debug("Position updated: {}, {}",
                                               coordinate.latitude(),
                                               coordinate.longitude());
                             }

                             position_ = info;

                             Q_EMIT self_->PositionUpdated(info);
                          });
      }
   }

   ~Impl() {}

   PositionManager* self_;

   boost::uuids::uuid trackingUuid_;
   bool               trackingEnabled_ {false};

   std::set<boost::uuids::uuid> uuids_ {};

   QGeoPositionInfoSource* geoPositionInfoSource_ {};
   QGeoPositionInfo        position_ {};
};

PositionManager::PositionManager() : p(std::make_unique<Impl>(this)) {}
PositionManager::~PositionManager() = default;

QGeoPositionInfo PositionManager::position() const
{
   return p->position_;
}

bool PositionManager::IsLocationTracked()
{
   return p->trackingEnabled_;
}

void PositionManager::EnablePositionUpdates(boost::uuids::uuid uuid,
                                            bool               enabled)
{
   if (p->geoPositionInfoSource_ == nullptr)
   {
      return;
   }

   if (enabled)
   {
      if (p->uuids_.empty())
      {
         p->geoPositionInfoSource_->startUpdates();
      }

      p->uuids_.insert(uuid);
   }
   else
   {
      p->uuids_.erase(uuid);

      if (p->uuids_.empty())
      {
         p->geoPositionInfoSource_->stopUpdates();
      }
   }
}

void PositionManager::TrackLocation(bool trackingEnabled)
{
   p->trackingEnabled_ = trackingEnabled;
   EnablePositionUpdates(p->trackingUuid_, trackingEnabled);
   Q_EMIT LocationTrackingChanged(trackingEnabled);
}

std::shared_ptr<PositionManager> PositionManager::Instance()
{
   static std::weak_ptr<PositionManager> positionManagerReference_ {};
   static std::mutex                     instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<PositionManager> positionManager =
      positionManagerReference_.lock();

   if (positionManager == nullptr)
   {
      positionManager           = std::make_shared<PositionManager>();
      positionManagerReference_ = positionManager;
   }

   return positionManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
