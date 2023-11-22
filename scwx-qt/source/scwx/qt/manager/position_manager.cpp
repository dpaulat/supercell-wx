#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <set>

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
   explicit Impl(PositionManager* self) : self_ {self}
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

void PositionManager::TrackLocation(boost::uuids::uuid uuid,
                                    bool               trackingEnabled)
{
   if (p->geoPositionInfoSource_ == nullptr)
   {
      return;
   }

   if (trackingEnabled)
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
