#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <boost/container/stable_vector.hpp>
#include <boost/container_hash/hash.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::alert_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

template<class Key>
struct AlertTypeHash;

template<>
struct AlertTypeHash<std::pair<awips::Phenomenon, bool>>
{
   size_t operator()(const std::pair<awips::Phenomenon, bool>& x) const;
};

class AlertLayerHandler : QObject
{
   Q_OBJECT
public:
   struct SegmentRecord
   {
      std::shared_ptr<const awips::Segment>            segment_;
      types::TextEventKey                              key_;
      std::shared_ptr<const awips::TextProductMessage> message_;
      std::chrono::system_clock::time_point            segmentBegin_;
      std::chrono::system_clock::time_point            segmentEnd_;

      SegmentRecord(
         const std::shared_ptr<const awips::Segment>&            segment,
         const types::TextEventKey&                              key,
         const std::shared_ptr<const awips::TextProductMessage>& message) :
          segment_ {segment},
          key_ {key},
          message_ {message},
          segmentBegin_ {segment->event_begin()},
          segmentEnd_ {segment->event_end()}
      {
      }
   };

   explicit AlertLayerHandler()
   {
      connect(textEventManager_.get(),
              &manager::TextEventManager::AlertUpdated,
              this,
              [this](const types::TextEventKey& key, std::size_t messageIndex)
              { HandleAlert(key, messageIndex); });
   }
   ~AlertLayerHandler()
   {
      disconnect(textEventManager_.get(), nullptr, this, nullptr);

      std::unique_lock lock(alertMutex_);
   }

   // NOTE: iterators are no longer stable if the stable vector moves
   std::unordered_map<
      std::pair<awips::Phenomenon, bool>,
      boost::container::stable_vector<std::shared_ptr<SegmentRecord>>,
      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      segmentsByType_ {};

   std::unordered_map<
      types::TextEventKey,
      boost::container::stable_vector<std::shared_ptr<SegmentRecord>>,
      types::TextEventHash<types::TextEventKey>>
      segmentsByKey_ {};

   void HandleAlert(const types::TextEventKey& key, size_t messageIndex);

   static AlertLayerHandler& Instance();

   std::shared_ptr<manager::TextEventManager> textEventManager_ {
      manager::TextEventManager::Instance()};

   std::mutex alertMutex_ {};

signals:
   void AlertsUpdated(awips::Phenomenon phenomenon, bool alertActive);
};

class AlertLayer::Impl
{
public:
   explicit Impl([[maybe_unused]] std::shared_ptr<MapContext> context,
                 awips::Phenomenon                            phenomenon) :
       phenomenon_ {phenomenon}
   {
   }
   ~Impl() {};

   awips::Phenomenon phenomenon_;
};

AlertLayer::AlertLayer(std::shared_ptr<MapContext> context,
                       awips::Phenomenon           phenomenon) :
    DrawLayer(context), p(std::make_unique<Impl>(context, phenomenon))
{
}

AlertLayer::~AlertLayer() = default;

void AlertLayer::Initialize()
{
   logger_->debug("Initialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Initialize();
}

void AlertLayer::Render(const QMapLibre::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void AlertLayer::Deinitialize()
{
   logger_->debug("Deinitialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Deinitialize();
}

bool AlertLayer::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2&                              mouseCoords,
   const common::Coordinate&                     mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&         eventHandler)
{
   return DrawLayer::RunMousePicking(params,
                                     mouseLocalPos,
                                     mouseGlobalPos,
                                     mouseCoords,
                                     mouseGeoCoords,
                                     eventHandler);
}

void AlertLayerHandler::HandleAlert(const types::TextEventKey& key,
                                    size_t                     messageIndex)
{
   logger_->trace("HandleAlert: {}", key.ToString());

   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertsUpdated {};

   auto message = textEventManager_->message_list(key).at(messageIndex);

   // Determine start time for first segment
   std::chrono::system_clock::time_point segmentBegin {};
   if (message->segment_count() > 0)
   {
      segmentBegin = message->segment(0)->event_begin();
   }

   // Take a unique mutex before modifying segments
   std::unique_lock lock {alertMutex_};

   // Update any existing segments with new end time
   auto& segmentsForKey = segmentsByKey_[key];
   for (auto& segmentRecord : segmentsForKey)
   {
      if (segmentRecord->segmentEnd_ > segmentBegin)
      {
         segmentRecord->segmentEnd_ = segmentBegin;
      }
   }

   // Process new segments
   for (auto& segment : message->segments())
   {
      if (!segment->codedLocation_.has_value())
      {
         // Cannot handle a segment without a location
         continue;
      }

      auto&             vtec       = segment->header_->vtecString_.front();
      auto              action     = vtec.pVtec_.action();
      awips::Phenomenon phenomenon = vtec.pVtec_.phenomenon();
      auto              eventEnd   = vtec.pVtec_.event_end();
      bool alertActive             = (action != awips::PVtec::Action::Canceled);

      auto& segmentsForType = segmentsByType_[{key.phenomenon_, alertActive}];

      // Insert segment into lists
      std::shared_ptr<SegmentRecord> segmentRecord =
         std::make_shared<SegmentRecord>(segment, key, message);

      segmentsForKey.push_back(segmentRecord);
      segmentsForType.push_back(segmentRecord);

      alertsUpdated.emplace(phenomenon, alertActive);
   }

   // Release the lock after completing segment updates
   lock.unlock();

   for (auto& alert : alertsUpdated)
   {
      // Emit signal for each updated alert type
      Q_EMIT AlertsUpdated(alert.first, alert.second);
   }
}

AlertLayerHandler& AlertLayerHandler::Instance()
{
   static AlertLayerHandler alertLayerHandler_ {};
   return alertLayerHandler_;
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
