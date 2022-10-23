#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/util/logger.hpp>

#include <unordered_set>

#include <boost/container_hash/hash.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::alert_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static void AddAlertLayer(std::shared_ptr<QMapLibreGL::Map> map,
                          const QString&                    idSuffix,
                          const QString&                    sourceId,
                          const QString&                    beforeLayer,
                          boost::gil::rgba8_pixel_t         outlineColor);
static QMapLibreGL::Feature
CreateFeature(const awips::CodedLocation& codedLocation);
static QMapLibreGL::Coordinate
GetMapboxCoordinate(const common::Coordinate& coordinate);
static QMapLibreGL::Coordinates
               GetMapboxCoordinates(const awips::CodedLocation& codedLocation);
static QString GetSuffix(awips::Phenomenon phenomenon, bool alertActive);

static const QVariantMap kEmptyFeatureCollection_ {
   {"type", "geojson"},
   {"data", QVariant::fromValue(std::list<QMapLibreGL::Feature> {})}};
static const std::list<awips::Phenomenon> kAlertPhenomena_ {
   awips::Phenomenon::Marine,
   awips::Phenomenon::FlashFlood,
   awips::Phenomenon::SevereThunderstorm,
   awips::Phenomenon::Tornado};

template<class Key>
struct AlertTypeHash;

template<>
struct AlertTypeHash<std::pair<awips::Phenomenon, bool>>
{
   size_t operator()(const std::pair<awips::Phenomenon, bool>& x) const;
};

class AlertLayerHandler : public QObject
{
   Q_OBJECT public :
       explicit AlertLayerHandler() :
       alertSourceMap_ {}, featureMap_ {}
   {
      for (auto& phenomenon : kAlertPhenomena_)
      {
         for (bool alertActive : {false, true})
         {
            alertSourceMap_.emplace(std::make_pair(phenomenon, alertActive),
                                    kEmptyFeatureCollection_);
         }
      }

      connect(&manager::TextEventManager::Instance(),
              &manager::TextEventManager::AlertUpdated,
              this,
              &AlertLayerHandler::HandleAlert,
              Qt::QueuedConnection);
   }
   ~AlertLayerHandler() = default;

   static AlertLayerHandler& Instance();

   std::list<QMapLibreGL::Feature>* FeatureList(awips::Phenomenon phenomenon,
                                                bool              alertActive);
   void HandleAlert(const types::TextEventKey& key, size_t messageIndex);

   std::unordered_map<std::pair<awips::Phenomenon, bool>,
                      QVariantMap,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertSourceMap_;
   std::unordered_multimap<
      types::TextEventKey,
      std::tuple<awips::Phenomenon,
                 bool,
                 std::list<QMapLibreGL::Feature>::iterator>,
      types::TextEventHash<types::TextEventKey>>
      featureMap_;

signals:
   void AlertsUpdated(awips::Phenomenon phenomenon, bool alertActive);
};

class AlertLayerImpl : public QObject
{
   Q_OBJECT
public:
   explicit AlertLayerImpl(std::shared_ptr<MapContext> context) :
       context_ {context}
   {
      connect(&AlertLayerHandler::Instance(),
              &AlertLayerHandler::AlertsUpdated,
              this,
              &AlertLayerImpl::UpdateSource);
   }
   ~AlertLayerImpl() = default;

   void UpdateSource(awips::Phenomenon phenomenon, bool alertActive);

   std::shared_ptr<MapContext> context_;
};

AlertLayer::AlertLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<AlertLayerImpl>(context))
{
}

AlertLayer::~AlertLayer() = default;

void AlertLayer::Initialize()
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize();
}

void AlertLayer::Render(const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void AlertLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

void AlertLayer::AddLayers(const std::string& before)
{
   logger_->debug("AddLayers()");

   auto map = p->context_->map().lock();
   if (map == nullptr)
   {
      return;
   }

   // Add/update GeoJSON sources
   for (auto& phenomenon : kAlertPhenomena_)
   {
      for (bool alertActive : {false, true})
      {
         p->UpdateSource(phenomenon, alertActive);
      }
   }

   const QString beforeLayer {QString::fromStdString(before)};

   // Create alert layers
   const QString ffActiveSuffix =
      GetSuffix(awips::Phenomenon::FlashFlood, true);
   const QString ffInactiveSuffix =
      GetSuffix(awips::Phenomenon::FlashFlood, false);
   const QString maActiveSuffix   = GetSuffix(awips::Phenomenon::Marine, true);
   const QString maInactiveSuffix = GetSuffix(awips::Phenomenon::Marine, false);
   const QString svActiveSuffix =
      GetSuffix(awips::Phenomenon::SevereThunderstorm, true);
   const QString svInactiveSuffix =
      GetSuffix(awips::Phenomenon::SevereThunderstorm, false);
   const QString toActiveSuffix = GetSuffix(awips::Phenomenon::Tornado, true);
   const QString toInactiveSuffix =
      GetSuffix(awips::Phenomenon::Tornado, false);

   AddAlertLayer(map,
                 maInactiveSuffix,
                 QString("alertPolygon-%1").arg(maInactiveSuffix),
                 beforeLayer,
                 {127, 63, 0, 255});
   AddAlertLayer(map,
                 maActiveSuffix,
                 QString("alertPolygon-%1").arg(maActiveSuffix),
                 beforeLayer,
                 {255, 127, 0, 255});
   AddAlertLayer(map,
                 ffInactiveSuffix,
                 QString("alertPolygon-%1").arg(ffInactiveSuffix),
                 beforeLayer,
                 {0, 127, 0, 255});
   AddAlertLayer(map,
                 ffActiveSuffix,
                 QString("alertPolygon-%1").arg(ffActiveSuffix),
                 beforeLayer,
                 {0, 255, 0, 255});
   AddAlertLayer(map,
                 svInactiveSuffix,
                 QString("alertPolygon-%1").arg(svInactiveSuffix),
                 beforeLayer,
                 {127, 127, 0, 255});
   AddAlertLayer(map,
                 svActiveSuffix,
                 QString("alertPolygon-%1").arg(svActiveSuffix),
                 beforeLayer,
                 {255, 255, 0, 255});
   AddAlertLayer(map,
                 toInactiveSuffix,
                 QString("alertPolygon-%1").arg(toInactiveSuffix),
                 beforeLayer,
                 {127, 0, 0, 255});
   AddAlertLayer(map,
                 toActiveSuffix,
                 QString("alertPolygon-%1").arg(toActiveSuffix),
                 beforeLayer,
                 {255, 0, 0, 255});
}

std::list<QMapLibreGL::Feature>*
AlertLayerHandler::FeatureList(awips::Phenomenon phenomenon, bool alertActive)
{
   std::list<QMapLibreGL::Feature>* featureList = nullptr;

   auto key = std::make_pair(phenomenon, alertActive);
   auto it  = alertSourceMap_.find(key);
   if (it != alertSourceMap_.cend())
   {
      featureList = reinterpret_cast<std::list<QMapLibreGL::Feature>*>(
         it->second["data"].data());
   }

   return featureList;
}

void AlertLayerHandler::HandleAlert(const types::TextEventKey& key,
                                    size_t                     messageIndex)
{
   auto message =
      manager::TextEventManager::Instance().message_list(key).at(messageIndex);
   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertsUpdated {};

   // Remove existing features for key
   auto existingFeatures = featureMap_.equal_range(key);
   for (auto it = existingFeatures.first; it != existingFeatures.second; ++it)
   {
      auto& [phenomenon, alertActive, featureIt] = it->second;
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
      bool alertActive             = (action != awips::PVtec::Action::Canceled);

      auto featureList = FeatureList(phenomenon, alertActive);
      if (featureList != nullptr)
      {
         // Add alert location to polygon list
         auto featureIt = featureList->emplace(
            featureList->cend(),
            CreateFeature(segment->codedLocation_.value()));

         // Store iterator for created feature in feature map
         featureMap_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(key),
            std::forward_as_tuple(phenomenon, alertActive, featureIt));

         // Mark alert type as updated
         alertsUpdated.emplace(phenomenon, alertActive);
      }
   }

   for (auto& alert : alertsUpdated)
   {
      // Emit signal for each updated alert type
      emit AlertsUpdated(alert.first, alert.second);
   }
}

void AlertLayerImpl::UpdateSource(awips::Phenomenon phenomenon,
                                  bool              alertActive)
{
   auto map = context_->map().lock();
   if (map == nullptr)
   {
      return;
   }

   // Update source, relies on alert source being defined
   map->updateSource(
      QString("alertPolygon-%1").arg(GetSuffix(phenomenon, alertActive)),
      AlertLayerHandler::Instance().alertSourceMap_.at(
         std::make_pair(phenomenon, alertActive)));
}

AlertLayerHandler& AlertLayerHandler::Instance()
{
   static AlertLayerHandler alertLayerHandler {};
   return alertLayerHandler;
}

static void AddAlertLayer(std::shared_ptr<QMapLibreGL::Map> map,
                          const QString&                    idSuffix,
                          const QString&                    sourceId,
                          const QString&                    beforeLayer,
                          boost::gil::rgba8_pixel_t         outlineColor)
{
   QString bgLayerId = QString("alertPolygonLayerBg-%1").arg(idSuffix);
   QString fgLayerId = QString("alertPolygonLayerFg-%1").arg(idSuffix);

   if (map->layerExists(bgLayerId))
   {
      map->removeLayer(bgLayerId);
   }
   if (map->layerExists(fgLayerId))
   {
      map->removeLayer(fgLayerId);
   }

   map->addLayer({{"id", bgLayerId}, {"type", "line"}, {"source", sourceId}},
                 beforeLayer);
   map->setLayoutProperty(bgLayerId, "line-join", "round");
   map->setLayoutProperty(bgLayerId, "line-cap", "round");
   map->setPaintProperty(bgLayerId, "line-color", "rgba(0, 0, 0, 255)");
   map->setPaintProperty(bgLayerId, "line-width", "5");

   map->addLayer({{"id", fgLayerId}, {"type", "line"}, {"source", sourceId}},
                 beforeLayer);
   map->setLayoutProperty(fgLayerId, "line-join", "round");
   map->setLayoutProperty(fgLayerId, "line-cap", "round");
   map->setPaintProperty(fgLayerId,
                         "line-color",
                         QString("rgba(%1, %2, %3, %4)")
                            .arg(outlineColor[0])
                            .arg(outlineColor[1])
                            .arg(outlineColor[2])
                            .arg(outlineColor[3]));
   map->setPaintProperty(fgLayerId, "line-width", "3");
}

static QMapLibreGL::Feature
CreateFeature(const awips::CodedLocation& codedLocation)
{
   auto mapboxCoordinates = GetMapboxCoordinates(codedLocation);

   return {QMapLibreGL::Feature::PolygonType,
           std::initializer_list<QMapLibreGL::CoordinatesCollection> {
              std::initializer_list<QMapLibreGL::Coordinates> {
                 {mapboxCoordinates}}}};
}

static QMapLibreGL::Coordinate
GetMapboxCoordinate(const common::Coordinate& coordinate)
{
   return {coordinate.latitude_, coordinate.longitude_};
}

static QMapLibreGL::Coordinates
GetMapboxCoordinates(const awips::CodedLocation& codedLocation)
{
   auto                     scwxCoordinates = codedLocation.coordinates();
   QMapLibreGL::Coordinates mapboxCoordinates(scwxCoordinates.size() + 1u);

   std::transform(scwxCoordinates.cbegin(),
                  scwxCoordinates.cend(),
                  mapboxCoordinates.begin(),
                  [](auto& coordinate) -> QMapLibreGL::Coordinate
                  { return GetMapboxCoordinate(coordinate); });

   mapboxCoordinates.back() = GetMapboxCoordinate(scwxCoordinates.front());

   return mapboxCoordinates;
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

#include "alert_layer.moc"
