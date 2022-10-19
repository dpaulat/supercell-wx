#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::alert_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static QMapbox::Coordinate
GetMapboxCoordinate(const common::Coordinate& coordinate);
static QMapbox::Coordinates
GetMapboxCoordinates(const awips::CodedLocation& codedLocation);

class AlertLayerHandler : public QObject
{
   Q_OBJECT
public:
   explicit AlertLayerHandler() :
       alertSource_ {{"type", "geojson"},
                     {"data", QVariant::fromValue(QList<QMapbox::Feature> {})}}
   {
      connect(&manager::TextEventManager::Instance(),
              &manager::TextEventManager::AlertUpdated,
              this,
              &AlertLayerHandler::HandleAlert);
   }
   ~AlertLayerHandler() = default;

   static AlertLayerHandler& Instance();

   QList<QMapbox::Feature>* FeatureList();
   void HandleAlert(const types::TextEventKey& key, size_t messageIndex);

   QVariantMap alertSource_;

signals:
   void AlertsUpdated();
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

   void UpdateSource();

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

void AlertLayer::Render(const QMapbox::CustomLayerRenderParameters& params)
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

   if (map->layerExists("alertPolygonLayerBg"))
   {
      map->removeLayer("alertPolygonLayerBg");
   }
   if (map->layerExists("alertPolygonLayerFg"))
   {
      map->removeLayer("alertPolygonLayerFg");
   }
   if (map->sourceExists("alertPolygon"))
   {
      map->removeSource("alertPolygon");
   }

   map->addSource("alertPolygon", AlertLayerHandler::Instance().alertSource_);

   map->addLayer({{"id", "alertPolygonLayerBg"},
                  {"type", "line"},
                  {"source", "alertPolygon"}},
                 QString::fromStdString(before));
   map->setLayoutProperty("alertPolygonLayerBg", "line-join", "round");
   map->setLayoutProperty("alertPolygonLayerBg", "line-cap", "round");
   map->setPaintProperty(
      "alertPolygonLayerBg", "line-color", "rgba(0, 0, 0, 255)");
   map->setPaintProperty("alertPolygonLayerBg", "line-width", "5");

   map->addLayer({{"id", "alertPolygonLayerFg"},
                  {"type", "line"},
                  {"source", "alertPolygon"}},
                 QString::fromStdString(before));
   map->setLayoutProperty("alertPolygonLayerFg", "line-join", "round");
   map->setLayoutProperty("alertPolygonLayerFg", "line-cap", "round");
   map->setPaintProperty(
      "alertPolygonLayerFg", "line-color", "rgba(255, 0, 0, 255)");
   map->setPaintProperty("alertPolygonLayerFg", "line-width", "3");
}

QList<QMapbox::Feature>* AlertLayerHandler::FeatureList()
{
   return reinterpret_cast<QList<QMapbox::Feature>*>(
      alertSource_["data"].data());
}

void AlertLayerHandler::HandleAlert(const types::TextEventKey& key,
                                    size_t                     messageIndex)
{
   auto message =
      manager::TextEventManager::Instance().message_list(key).at(messageIndex);
   bool alertUpdated = false;

   // TODO: Remove previous items

   for (auto segment : message->segments())
   {
      if (!segment->codedLocation_.has_value())
      {
         continue;
      }

      // Add alert location to polygon list
      auto mapboxCoordinates =
         GetMapboxCoordinates(segment->codedLocation_.value());

      FeatureList()->push_back(
         {QMapbox::Feature::PolygonType,
          std::initializer_list<QMapbox::CoordinatesCollection> {
             std::initializer_list<QMapbox::Coordinates> {
                {mapboxCoordinates}}}});

      alertUpdated = true;
   }

   if (alertUpdated)
   {
      emit AlertsUpdated();
   }
}

void AlertLayerImpl::UpdateSource()
{
   auto map = context_->map().lock();
   if (map == nullptr)
   {
      return;
   }

   map->updateSource("alertPolygon",
                     AlertLayerHandler::Instance().alertSource_);
}

AlertLayerHandler& AlertLayerHandler::Instance()
{
   static AlertLayerHandler alertLayerHandler {};
   return alertLayerHandler;
}

static QMapbox::Coordinate
GetMapboxCoordinate(const common::Coordinate& coordinate)
{
   return {coordinate.latitude_, coordinate.longitude_};
}

static QMapbox::Coordinates
GetMapboxCoordinates(const awips::CodedLocation& codedLocation)
{
   auto                 scwxCoordinates = codedLocation.coordinates();
   QMapbox::Coordinates mapboxCoordinates(scwxCoordinates.size() + 1u);

   std::transform(scwxCoordinates.cbegin(),
                  scwxCoordinates.cend(),
                  mapboxCoordinates.begin(),
                  [](auto& coordinate) -> QMapbox::Coordinate
                  { return GetMapboxCoordinate(coordinate); });

   mapboxCoordinates.back() = GetMapboxCoordinate(scwxCoordinates.front());

   return mapboxCoordinates;
}

} // namespace map
} // namespace qt
} // namespace scwx

#include "alert_layer.moc"
