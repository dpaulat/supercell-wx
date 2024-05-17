#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/location_types.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <mutex>
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
      auto& generalSettings = settings::GeneralSettings::Instance();

      logger_->debug(
         "Available sources: {}",
         QGeoPositionInfoSource::availableSources().join(", ").toStdString());

      CreatePositionSource();

      positioningPluginCallbackUuid_ =
         generalSettings.positioning_plugin().RegisterValueChangedCallback(
            [this](const std::string&)
            { createPositionSourcePending_ = true; });
      nmeaBaudRateCallbackUuid_ =
         generalSettings.nmea_baud_rate().RegisterValueChangedCallback(
            [this](const std::int64_t&)
            { createPositionSourcePending_ = true; });
      nmeaSourceCallbackUuid_ =
         generalSettings.nmea_source().RegisterValueChangedCallback(
            [this](const std::string&)
            { createPositionSourcePending_ = true; });

      connect(&SettingsManager::Instance(),
              &SettingsManager::SettingsSaved,
              self_,
              [this]()
              {
                 if (createPositionSourcePending_)
                 {
                    CreatePositionSource();
                 }
              });
   }
   ~Impl()
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      generalSettings.positioning_plugin().UnregisterValueChangedCallback(
         positioningPluginCallbackUuid_);
      generalSettings.nmea_baud_rate().UnregisterValueChangedCallback(
         nmeaBaudRateCallbackUuid_);
      generalSettings.nmea_source().UnregisterValueChangedCallback(
         nmeaSourceCallbackUuid_);
   }

   void CreatePositionSource();

   PositionManager* self_;

   boost::uuids::uuid trackingUuid_;
   bool               trackingEnabled_ {false};

   std::set<boost::uuids::uuid> uuids_ {};

   std::mutex positionSourceMutex_ {};

   QGeoPositionInfoSource* geoPositionInfoSource_ {};
   QGeoPositionInfo        position_ {};

   types::PositioningPlugin lastPositioningPlugin_ {
      types::PositioningPlugin::Unknown};
   std::int64_t lastNmeaBaudRate_ {-1};
   std::string  lastNmeaSource_ {"?"};

   boost::uuids::uuid positioningPluginCallbackUuid_ {};
   boost::uuids::uuid nmeaBaudRateCallbackUuid_ {};
   boost::uuids::uuid nmeaSourceCallbackUuid_ {};

   bool createPositionSourcePending_ {false};
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

void PositionManager::Impl::CreatePositionSource()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   createPositionSourcePending_ = false;

   types::PositioningPlugin positioningPlugin = types::GetPositioningPlugin(
      generalSettings.positioning_plugin().GetValue());
   std::int64_t nmeaBaudRate = generalSettings.nmea_baud_rate().GetValue();
   std::string  nmeaSource   = generalSettings.nmea_source().GetValue();

   if (positioningPlugin == lastPositioningPlugin_ &&
       nmeaBaudRate == lastNmeaBaudRate_ && nmeaSource == lastNmeaSource_)
   {
      return;
   }

   QGeoPositionInfoSource* positionSource = nullptr;

   // TODO: macOS requires permission
   if (positioningPlugin == types::PositioningPlugin::Default)
   {
      positionSource = QGeoPositionInfoSource::createDefaultSource(self_);
   }
   else if (positioningPlugin == types::PositioningPlugin::Nmea)
   {
      QVariantMap params {};
      params["nmea.source"]   = QString::fromStdString(nmeaSource);
      params["nmea.baudrate"] = static_cast<int>(nmeaBaudRate);

      positionSource =
         QGeoPositionInfoSource::createSource("nmea", params, self_);
   }

   if (positionSource != nullptr)
   {
      logger_->debug("Using position source: {}",
                     positionSource->sourceName().toStdString());

      QObject::connect(positionSource,
                       &QGeoPositionInfoSource::positionUpdated,
                       self_,
                       [this](const QGeoPositionInfo& info)
                       {
                          auto coordinate = info.coordinate();

                          if (coordinate != position_.coordinate())
                          {
                             logger_->trace("Position updated: {}, {}",
                                            coordinate.latitude(),
                                            coordinate.longitude());
                          }

                          position_ = info;

                          Q_EMIT self_->PositionUpdated(info);
                       });
   }
   else
   {
      logger_->error("Unable to create position source for plugin: {}",
                     types::GetPositioningPluginName(positioningPlugin));
      return;
   }

   lastPositioningPlugin_ = positioningPlugin;
   lastNmeaBaudRate_      = nmeaBaudRate;
   lastNmeaSource_        = nmeaSource;

   std::unique_lock lock {positionSourceMutex_};

   if (geoPositionInfoSource_ != nullptr)
   {
      geoPositionInfoSource_->stopUpdates();
      delete geoPositionInfoSource_;
   }

   geoPositionInfoSource_ = positionSource;

   if (!uuids_.empty())
   {
      positionSource->startUpdates();
   }
}

void PositionManager::EnablePositionUpdates(boost::uuids::uuid uuid,
                                            bool               enabled)
{
   std::unique_lock lock {p->positionSourceMutex_};

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
