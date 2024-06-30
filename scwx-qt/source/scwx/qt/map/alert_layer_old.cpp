#include <scwx/qt/map/alert_layer_old.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/types/layer_types.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <chrono>
#include <shared_mutex>
#include <unordered_set>

#include <boost/asio/steady_timer.hpp>
#include <boost/container_hash/hash.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::alert_layer_old";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static std::vector<std::string>
AddAlertLayer(std::shared_ptr<QMapLibre::Map> map,
              awips::Phenomenon               phenomenon,
              bool                            alertActive,
              const QString&                  beforeLayer);
static QMapLibre::Feature
CreateFeature(const awips::CodedLocation& codedLocation);
static QMapLibre::Coordinate
GetMapboxCoordinate(const common::Coordinate& coordinate);
static QMapLibre::Coordinates
               GetMapboxCoordinates(const awips::CodedLocation& codedLocation);
static QString GetSourceId(awips::Phenomenon phenomenon, bool alertActive);
static QString GetSuffix(awips::Phenomenon phenomenon, bool alertActive);

static const QVariantMap kEmptyFeatureCollection_ {
   {"type", "geojson"},
   {"data", QVariant::fromValue(std::list<QMapLibre::Feature> {})}};
static const std::vector<awips::Phenomenon> kAlertPhenomena_ {
   awips::Phenomenon::Marine,
   awips::Phenomenon::FlashFlood,
   awips::Phenomenon::SevereThunderstorm,
   awips::Phenomenon::SnowSquall,
   awips::Phenomenon::Tornado};

template<class Key>
struct AlertTypeHash;

template<>
struct AlertTypeHash<std::pair<awips::Phenomenon, bool>>
{
   size_t operator()(const std::pair<awips::Phenomenon, bool>& x) const;
};

class AlertLayerOldHandler : public QObject
{
   Q_OBJECT public :
       explicit AlertLayerOldHandler() :
       textEventManager_ {manager::TextEventManager::Instance()},
       alertUpdateTimer_ {scwx::util::io_context()},
       alertSourceMap_ {},
       featureMap_ {}
   {
      for (auto& phenomenon : kAlertPhenomena_)
      {
         for (bool alertActive : {false, true})
         {
            alertSourceMap_.emplace(std::make_pair(phenomenon, alertActive),
                                    kEmptyFeatureCollection_);
         }
      }

      connect(textEventManager_.get(),
              &manager::TextEventManager::AlertUpdated,
              this,
              &AlertLayerOldHandler::HandleAlert);
   }
   ~AlertLayerOldHandler()
   {
      std::unique_lock lock(alertMutex_);
      alertUpdateTimer_.cancel();
   }

   static std::shared_ptr<AlertLayerOldHandler> Instance();

   std::list<QMapLibre::Feature>* FeatureList(awips::Phenomenon phenomenon,
                                              bool              alertActive);

   void HandleAlert(const types::TextEventKey& key, size_t messageIndex);
   void UpdateAlerts();

   std::shared_ptr<manager::TextEventManager> textEventManager_;

   boost::asio::steady_timer alertUpdateTimer_;
   std::unordered_map<std::pair<awips::Phenomenon, bool>,
                      QVariantMap,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertSourceMap_;
   std::unordered_multimap<types::TextEventKey,
                           std::tuple<awips::Phenomenon,
                                      bool,
                                      std::list<QMapLibre::Feature>::iterator,
                                      std::chrono::system_clock::time_point>,
                           types::TextEventHash<types::TextEventKey>>
                     featureMap_;
   std::shared_mutex alertMutex_;

signals:
   void AlertsUpdated(awips::Phenomenon phenomenon, bool alertActive);
};

class AlertLayerOldImpl : public QObject
{
   Q_OBJECT
public:
   explicit AlertLayerOldImpl(std::shared_ptr<MapContext> context) :
       context_ {context},
       alertLayerOldHandler_ {AlertLayerOldHandler::Instance()}
   {
      connect(alertLayerOldHandler_.get(),
              &AlertLayerOldHandler::AlertsUpdated,
              this,
              &AlertLayerOldImpl::UpdateSource);
   }
   ~AlertLayerOldImpl() {};

   void UpdateSource(awips::Phenomenon phenomenon, bool alertActive);

   std::shared_ptr<MapContext>           context_;
   std::shared_ptr<AlertLayerOldHandler> alertLayerOldHandler_;
};

AlertLayerOld::AlertLayerOld(std::shared_ptr<MapContext> context) :
    p(std::make_unique<AlertLayerOldImpl>(context))
{
}

AlertLayerOld::~AlertLayerOld() = default;

std::vector<std::string> AlertLayerOld::AddLayers(awips::Phenomenon  phenomenon,
                                                  const std::string& before)
{
   logger_->debug("AddLayers(): {}", awips::GetPhenomenonCode(phenomenon));

   std::vector<std::string> layers {};

   auto map = p->context_->map().lock();
   if (map == nullptr)
   {
      return layers;
   }

   const QString beforeLayer {QString::fromStdString(before)};

   // Add/update GeoJSON sources and create layers
   for (bool alertActive : {false, true})
   {
      p->UpdateSource(phenomenon, alertActive);
      auto newLayers = AddAlertLayer(map, phenomenon, alertActive, beforeLayer);
      layers.insert(layers.end(), newLayers.cbegin(), newLayers.cend());
   }

   return layers;
}

std::list<QMapLibre::Feature>*
AlertLayerOldHandler::FeatureList(awips::Phenomenon phenomenon,
                                  bool              alertActive)
{
   std::list<QMapLibre::Feature>* featureList = nullptr;

   auto key = std::make_pair(phenomenon, alertActive);
   auto it  = alertSourceMap_.find(key);
   if (it != alertSourceMap_.cend())
   {
      featureList = reinterpret_cast<std::list<QMapLibre::Feature>*>(
         it->second["data"].data());
   }

   return featureList;
}

void AlertLayerOldHandler::HandleAlert(const types::TextEventKey& key,
                                       size_t                     messageIndex)
{
   // Skip alert if there are more messages to be processed
   if (messageIndex + 1 < textEventManager_->message_count(key))
   {
      return;
   }

   auto message = textEventManager_->message_list(key).at(messageIndex);
   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertsUpdated {};

   // Take a unique lock before modifying feature lists
   std::unique_lock lock(alertMutex_);

   // Remove existing features for key
   auto existingFeatures = featureMap_.equal_range(key);
   for (auto it = existingFeatures.first; it != existingFeatures.second; ++it)
   {
      auto& [phenomenon, alertActive, featureIt, eventEnd] = it->second;
      auto featureList = FeatureList(phenomenon, alertActive);
      if (featureList != nullptr)
      {
         // Remove existing feature for key
         featureList->erase(featureIt);

         // Mark alert type as updated
         alertsUpdated.emplace(phenomenon, alertActive);
      }
   }
   featureMap_.erase(existingFeatures.first, existingFeatures.second);

   for (auto segment : message->segments())
   {
      if (!segment->codedLocation_.has_value())
      {
         continue;
      }

      auto&             vtec       = segment->header_->vtecString_.front();
      auto              action     = vtec.pVtec_.action();
      awips::Phenomenon phenomenon = vtec.pVtec_.phenomenon();
      auto              eventEnd   = vtec.pVtec_.event_end();
      bool alertActive             = (action != awips::PVtec::Action::Canceled);

      // If the event has ended, skip it
      if (eventEnd < std::chrono::system_clock::now())
      {
         continue;
      }

      auto featureList = FeatureList(phenomenon, alertActive);
      if (featureList != nullptr)
      {
         // Add alert location to polygon list
         auto featureIt = featureList->emplace(
            featureList->cend(),
            CreateFeature(segment->codedLocation_.value()));

         // Store iterator for created feature in feature map
         featureMap_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(key),
                             std::forward_as_tuple(
                                phenomenon, alertActive, featureIt, eventEnd));

         // Mark alert type as updated
         alertsUpdated.emplace(phenomenon, alertActive);
      }
   }

   // Release the lock after completing feature list updates
   lock.unlock();

   for (auto& alert : alertsUpdated)
   {
      // Emit signal for each updated alert type
      Q_EMIT AlertsUpdated(alert.first, alert.second);
   }
}

void AlertLayerOldHandler::UpdateAlerts()
{
   logger_->trace("UpdateAlerts");

   // Take a unique lock before modifying feature lists
   std::unique_lock lock(alertMutex_);

   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertsUpdated {};

   // Evaluate each rendered feature for expiration
   for (auto it = featureMap_.begin(); it != featureMap_.end();)
   {
      auto& [phenomenon, alertActive, featureIt, eventEnd] = it->second;

      // If the event has ended, remove it from the feature list
      if (eventEnd < std::chrono::system_clock::now())
      {
         logger_->debug("Alert expired: {}", it->first.ToString());

         auto featureList = FeatureList(phenomenon, alertActive);
         if (featureList != nullptr)
         {
            // Remove existing feature for key
            featureList->erase(featureIt);

            // Mark alert type as updated
            alertsUpdated.emplace(phenomenon, alertActive);
         }

         // Erase current item and increment iterator
         it = featureMap_.erase(it);
      }
      else
      {
         // Current item is not expired, continue
         ++it;
      }
   }

   for (auto& alert : alertsUpdated)
   {
      // Emit signal for each updated alert type
      Q_EMIT AlertsUpdated(alert.first, alert.second);
   }

   using namespace std::chrono;
   alertUpdateTimer_.expires_after(15s);
   alertUpdateTimer_.async_wait(
      [this](const boost::system::error_code& e)
      {
         if (e == boost::asio::error::operation_aborted)
         {
            logger_->debug("Alert update timer cancelled");
         }
         else if (e != boost::system::errc::success)
         {
            logger_->warn("Alert update timer error: {}", e.message());
         }
         else
         {
            try
            {
               UpdateAlerts();
            }
            catch (const std::exception& ex)
            {
               logger_->error(ex.what());
            }
         }
      });
}

void AlertLayerOldImpl::UpdateSource(awips::Phenomenon phenomenon,
                                     bool              alertActive)
{
   auto map = context_->map().lock();
   if (map == nullptr)
   {
      return;
   }

   // Take a shared lock before using feature lists
   std::shared_lock lock(alertLayerOldHandler_->alertMutex_);

   // Update source, relies on alert source being defined
   map->updateSource(GetSourceId(phenomenon, alertActive),
                     alertLayerOldHandler_->alertSourceMap_.at(
                        std::make_pair(phenomenon, alertActive)));
}

std::shared_ptr<AlertLayerOldHandler> AlertLayerOldHandler::Instance()
{
   static std::weak_ptr<AlertLayerOldHandler> alertLayerOldHandlerReference {};
   static std::mutex                          instanceMutex {};

   std::unique_lock lock(instanceMutex);

   std::shared_ptr<AlertLayerOldHandler> alertLayerOldHandler =
      alertLayerOldHandlerReference.lock();

   if (alertLayerOldHandler == nullptr)
   {
      alertLayerOldHandler          = std::make_shared<AlertLayerOldHandler>();
      alertLayerOldHandlerReference = alertLayerOldHandler;

      alertLayerOldHandler->UpdateAlerts();
   }

   return alertLayerOldHandler;
}

static std::vector<std::string>
AddAlertLayer(std::shared_ptr<QMapLibre::Map> map,
              awips::Phenomenon               phenomenon,
              bool                            alertActive,
              const QString&                  beforeLayer)
{
   settings::PaletteSettings& paletteSettings =
      settings::PaletteSettings::Instance();

   QString layerPrefix = QString::fromStdString(
      types::GetLayerName(types::LayerType::Alert, phenomenon));

   QString sourceId     = GetSourceId(phenomenon, alertActive);
   QString idSuffix     = GetSuffix(phenomenon, alertActive);
   auto    outlineColor = util::color::ToRgba8PixelT(
      paletteSettings.alert_color(phenomenon, alertActive).GetValue());

   QString bgLayerId = QString("%1::bg-%2").arg(layerPrefix).arg(idSuffix);
   QString fgLayerId = QString("%1::fg-%2").arg(layerPrefix).arg(idSuffix);

   if (map->layerExists(bgLayerId))
   {
      map->removeLayer(bgLayerId);
   }
   if (map->layerExists(fgLayerId))
   {
      map->removeLayer(fgLayerId);
   }

   const float opacity = outlineColor[3] / 255.0f;

   map->addLayer(
      bgLayerId, {{"type", "line"}, {"source", sourceId}}, beforeLayer);
   map->setLayoutProperty(bgLayerId, "line-join", "round");
   map->setLayoutProperty(bgLayerId, "line-cap", "round");
   map->setPaintProperty(bgLayerId, "line-color", "rgba(0, 0, 0, 255)");
   map->setPaintProperty(bgLayerId, "line-opacity", QString("%1").arg(opacity));
   map->setPaintProperty(bgLayerId, "line-width", "5");

   map->addLayer(
      fgLayerId, {{"type", "line"}, {"source", sourceId}}, beforeLayer);
   map->setLayoutProperty(fgLayerId, "line-join", "round");
   map->setLayoutProperty(fgLayerId, "line-cap", "round");
   map->setPaintProperty(fgLayerId,
                         "line-color",
                         QString("rgba(%1, %2, %3, %4)")
                            .arg(outlineColor[0])
                            .arg(outlineColor[1])
                            .arg(outlineColor[2])
                            .arg(outlineColor[3]));
   map->setPaintProperty(fgLayerId, "line-opacity", QString("%1").arg(opacity));
   map->setPaintProperty(fgLayerId, "line-width", "3");

   return {bgLayerId.toStdString(), fgLayerId.toStdString()};
}

static QMapLibre::Feature
CreateFeature(const awips::CodedLocation& codedLocation)
{
   auto mapboxCoordinates = GetMapboxCoordinates(codedLocation);

   return QMapLibre::Feature {
      QMapLibre::Feature::PolygonType,
      std::initializer_list<QMapLibre::CoordinatesCollection> {
         std::initializer_list<QMapLibre::Coordinates> {{mapboxCoordinates}}}};
}

static QMapLibre::Coordinate
GetMapboxCoordinate(const common::Coordinate& coordinate)
{
   return {coordinate.latitude_, coordinate.longitude_};
}

static QMapLibre::Coordinates
GetMapboxCoordinates(const awips::CodedLocation& codedLocation)
{
   auto                   scwxCoordinates = codedLocation.coordinates();
   QMapLibre::Coordinates mapboxCoordinates(scwxCoordinates.size() + 1u);

   std::transform(scwxCoordinates.cbegin(),
                  scwxCoordinates.cend(),
                  mapboxCoordinates.begin(),
                  [](auto& coordinate) -> QMapLibre::Coordinate
                  { return GetMapboxCoordinate(coordinate); });

   mapboxCoordinates.back() = GetMapboxCoordinate(scwxCoordinates.front());

   return mapboxCoordinates;
}

static QString GetSourceId(awips::Phenomenon phenomenon, bool alertActive)
{
   return QString("alertPolygon-%1").arg(GetSuffix(phenomenon, alertActive));
}

static QString GetSuffix(awips::Phenomenon phenomenon, bool alertActive)
{
   return QString("-%1.%2")
      .arg(QString::fromStdString(awips::GetPhenomenonCode(phenomenon)))
      .arg(alertActive);
}

size_t AlertTypeHash<std::pair<awips::Phenomenon, bool>>::operator()(
   const std::pair<awips::Phenomenon, bool>& x) const
{
   size_t seed = 0;
   boost::hash_combine(seed, x.first);
   boost::hash_combine(seed, x.second);
   return seed;
}

} // namespace map
} // namespace qt
} // namespace scwx

#include "alert_layer_old.moc"
