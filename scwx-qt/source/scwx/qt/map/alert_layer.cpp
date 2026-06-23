#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/map/geo_stroke.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <boost/algorithm/string/join.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container_hash/hash.hpp>
#include <QEvent>
#include <QTimer>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::alert_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const boost::gil::rgba32f_pixel_t kBlack_ {0.0f, 0.0f, 0.0f, 1.0f};

template<class Key>
struct AlertTypeHash;

template<>
struct AlertTypeHash<std::pair<awips::Phenomenon, bool>>
{
   size_t operator()(const std::pair<awips::Phenomenon, bool>& x) const;
};

static bool IsAlertActive(const std::shared_ptr<const awips::Segment>& segment);

class AlertLayerHandler : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(AlertLayerHandler)

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
         types::TextEventKey                                     key,
         const std::shared_ptr<const awips::TextProductMessage>& message) :
          segment_ {segment},
          key_ {std::move(key)},
          message_ {message},
          segmentBegin_ {segment->event_begin()},
          segmentEnd_ {segment->event_end()}
      {
      }
   };

   explicit AlertLayerHandler()
   {
      renderCoalesceTimer_.setSingleShot(true);
      renderCoalesceTimer_.setInterval(33);
      connect(&renderCoalesceTimer_,
              &QTimer::timeout,
              this,
              &AlertLayerHandler::FlushQueuedAlertsNeedRendering);

      connect(textEventManager_.get(),
              &manager::TextEventManager::AlertUpdated,
              this,
              &AlertLayerHandler::HandleAlert);
      connect(textEventManager_.get(),
              &manager::TextEventManager::AlertsRemoved,
              this,
              &AlertLayerHandler::HandleAlertsRemoved);
      connect(textEventManager_.get(),
              &manager::TextEventManager::BulkAlertLoadStarted,
              this,
              &AlertLayerHandler::BeginBulkLoad);
      connect(textEventManager_.get(),
              &manager::TextEventManager::BulkAlertLoadFinished,
              this,
              &AlertLayerHandler::EndBulkLoad);

      std::call_once(refreshScheduled_, [this]() { ScheduleRefresh(); });
   }
   ~AlertLayerHandler()
   {
      disconnect(textEventManager_.get(), nullptr, this, nullptr);

      {
         std::unique_lock refreshLock(refreshMutex_);
         refreshEnabled_ = false;
         refreshTimer_.cancel();
      }

      refreshThreadPool_.stop();
      refreshThreadPool_.join();

      std::unique_lock lock(alertMutex_);
   }

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

   void HandleAlert(const types::TextEventKey& key,
                    size_t                     messageIndex,
                    boost::uuids::uuid         uuid);
   void HandleAlertsRemoved(
      const std::unordered_set<types::TextEventKey,
                               types::TextEventHash<types::TextEventKey>>&
         keys);

   static AlertLayerHandler& Instance();

   [[nodiscard]] std::shared_ptr<gl::draw::GeoLines>
   SharedGeoLines(awips::Phenomenon                             phenomenon,
                  bool                                          alertActive,
                  const std::shared_ptr<render::RenderContext>& renderContext);

   [[nodiscard]] bool TryBeginPopulate(awips::Phenomenon phenomenon);
   void               EndPopulate(awips::Phenomenon phenomenon);

   [[nodiscard]] bool
   TryTrackSegment(const std::shared_ptr<SegmentRecord>& segmentRecord);

   void RepopulateGeometry(awips::Phenomenon phenomenon);

   void ClearTrackedSegments();

   [[nodiscard]] std::optional<types::TextEventKey>
   KeyForGeoLine(const std::shared_ptr<const gl::draw::GeoLineDrawItem>& line);

   void ShowGeoLineHoverTooltip(
      awips::Phenomenon                                       phenomenon,
      const std::shared_ptr<const gl::draw::GeoLineDrawItem>& line,
      const QPointF&                                          mouseGlobalPos);

   [[nodiscard]] std::size_t SharedGeoLineCount();

   void RegisterGeometryBuilder(awips::Phenomenon phenomenon,
                                AlertLayer*       layer);
   void UnregisterGeometryBuilder(awips::Phenomenon phenomenon,
                                  AlertLayer*       layer);

   void QueueAlertsNeedRendering(awips::Phenomenon phenomenon);

   [[nodiscard]] bool InBulkLoad() const noexcept
   {
      return bulkLoadDepth_.load(std::memory_order_acquire) > 0;
   }

   struct SharedPhenomenonGeometry
   {
      awips::Phenomenon phenomenon {};
      std::unordered_map<bool, std::shared_ptr<gl::draw::GeoLines>>
         geoLines_ {};
      std::unordered_map<std::shared_ptr<const SegmentRecord>,
                         boost::container::stable_vector<
                            std::shared_ptr<gl::draw::GeoLineDrawItem>>>
         linesBySegment_ {};
      std::unordered_map<std::shared_ptr<const gl::draw::GeoLineDrawItem>,
                         std::shared_ptr<const SegmentRecord>>
                                                     segmentsByLine_ {};
      std::weak_ptr<const gl::draw::GeoLineDrawItem> lastHoverLine_ {};
      std::string                                    hoverTooltip_ {};
      std::mutex                                     linesMutex_ {};
      bool                                           populated_ {false};
   };

   [[nodiscard]] SharedPhenomenonGeometry&
   SharedGeometry(awips::Phenomenon                             phenomenon,
                  const std::shared_ptr<render::RenderContext>& renderContext);

   std::shared_ptr<manager::TextEventManager> textEventManager_ {
      manager::TextEventManager::Instance()};

   std::shared_mutex alertMutex_ {};

private:
   void ScheduleRefresh();
   void FlushQueuedAlertsNeedRendering();
   void BeginBulkLoad();
   void EndBulkLoad();

   std::atomic<int> bulkLoadDepth_ {0};
   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
              bulkFinishPending_ {};
   std::mutex bulkFinishMutex_ {};

   std::unordered_map<awips::Phenomenon, SharedPhenomenonGeometry> geometry_ {};
   std::mutex                                               geometryMutex_ {};
   std::unordered_set<std::shared_ptr<const SegmentRecord>> trackedSegments_ {};
   std::mutex                                         trackedSegmentsMutex_ {};
   std::unordered_map<awips::Phenomenon, AlertLayer*> geometryBuilders_ {};
   std::mutex                                         geometryBuildersMutex_ {};

   boost::asio::thread_pool  refreshThreadPool_ {1u};
   std::atomic<bool>         refreshEnabled_ {true};
   boost::asio::system_timer refreshTimer_ {refreshThreadPool_};
   std::mutex                refreshMutex_ {};
   std::once_flag            refreshScheduled_ {};

   QTimer                                renderCoalesceTimer_ {};
   std::mutex                            renderCoalesceMutex_ {};
   std::unordered_set<awips::Phenomenon> renderCoalescePending_ {};

signals:
   void AlertsNeedRendering(awips::Phenomenon phenomenon);
   void AlertAdded(const std::shared_ptr<SegmentRecord>& segmentRecord,
                   awips::Phenomenon                     phenomenon);
   void AlertUpdated(const std::shared_ptr<SegmentRecord>& segmentRecord);
   void AlertsRemoved(awips::Phenomenon phenomenon);
   void AlertsUpdated(awips::Phenomenon phenomenon, bool alertActive);
};

class AlertLayer::Impl
{
public:
   struct LineData
   {
      boost::gil::rgba32f_pixel_t borderColor_ {};
      boost::gil::rgba32f_pixel_t highlightColor_ {};
      boost::gil::rgba32f_pixel_t lineColor_ {};

      std::size_t borderWidth_ {};
      std::size_t highlightWidth_ {};
      std::size_t lineWidth_ {};
   };

   explicit Impl(AlertLayer*                                   self,
                 const std::shared_ptr<render::RenderContext>& renderContext,
                 awips::Phenomenon                             phenomenon) :
       self_ {self},
       phenomenon_ {phenomenon},
       ibw_ {awips::ibw::GetImpactBasedWarningInfo(phenomenon)},
       renderContext_ {renderContext}
   {
      UpdateLineData();
      ConnectSignals();
   }
   ~Impl()
   {
      threadPool_.stop();
      threadPool_.join();

      receiver_ = nullptr;
   };

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void AddAlert(
      const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord);
   void UpdateAlert(
      const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord);
   void ConnectAlertHandlerSignals();
   void ConnectSignals();
   void HandleGeoLinesEvent(std::weak_ptr<gl::draw::GeoLineDrawItem> di,
                            QEvent*                                  ev);

   [[nodiscard]] AlertLayerHandler::SharedPhenomenonGeometry& SharedGeometry();

   LineData& GetLineData(const std::shared_ptr<const awips::Segment>& segment,
                         bool alertActive);
   void      UpdateLineData();

   void AddLine(std::shared_ptr<gl::draw::GeoLines>&        geoLines,
                std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                const common::Coordinate&                   p1,
                const common::Coordinate&                   p2,
                const boost::gil::rgba32f_pixel_t&          color,
                float                                       width,
                std::chrono::system_clock::time_point       startTime,
                std::chrono::system_clock::time_point       endTime,
                bool                                        enableHover);
   void AddStyledLine(std::shared_ptr<gl::draw::GeoLines>&        geoLines,
                      std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                      const common::Coordinate&                   p1,
                      const common::Coordinate&                   p2,
                      const LineData&                             lineData,
                      std::chrono::system_clock::time_point       startTime,
                      std::chrono::system_clock::time_point       endTime,
                      bool                                        enableHover);
   void
        AddStyledLines(std::shared_ptr<gl::draw::GeoLines>&   geoLines,
                       const std::vector<common::Coordinate>& coordinates,
                       const LineData&                        lineData,
                       std::chrono::system_clock::time_point  startTime,
                       std::chrono::system_clock::time_point  endTime,
                       bool                                   enableHover,
                       boost::container::stable_vector<
                          std::shared_ptr<gl::draw::GeoLineDrawItem>>& drawItems);
   void AddLines(std::shared_ptr<gl::draw::GeoLines>&   geoLines,
                 const std::vector<common::Coordinate>& coordinates,
                 const boost::gil::rgba32f_pixel_t&     color,
                 float                                  width,
                 std::chrono::system_clock::time_point  startTime,
                 std::chrono::system_clock::time_point  endTime,
                 bool                                   enableHover,
                 boost::container::stable_vector<
                    std::shared_ptr<gl::draw::GeoLineDrawItem>>& drawItems);
   void PopulateLines(bool alertActive);
   void RepopulateLines();
   void UpdateLines();
   void FinishGeoLines(bool alertActive);

   static LineData CreateLineData(const settings::LineSettings& lineSettings);

   boost::asio::thread_pool threadPool_ {1u};

   AlertLayer* self_;

   const awips::Phenomenon                   phenomenon_;
   const awips::ibw::ImpactBasedWarningInfo& ibw_;

   std::shared_ptr<render::RenderContext> renderContext_;

   std::unique_ptr<QObject> receiver_ {std::make_unique<QObject>()};
   std::mutex               receiverMutex_ {};

   std::unordered_map<awips::ibw::ThreatCategory, LineData>
            threatCategoryLineData_;
   LineData observedLineData_ {};
   LineData tornadoPossibleLineData_ {};
   LineData inactiveLineData_ {};

   std::chrono::system_clock::time_point selectedTime_ {};
   bool                                  suppressNeedsRendering_ {false};
   bool                                  handlerSignalsConnected_ {false};

   std::vector<boost::signals2::scoped_connection> connections_ {};
};

AlertLayer::AlertLayer(
   const std::shared_ptr<render::RenderContext>& renderContext,
   awips::Phenomenon                             phenomenon) :
    DrawLayer(
       renderContext,
       fmt::format("AlertLayer {}", awips::GetPhenomenonText(phenomenon))),
    p(std::make_unique<Impl>(this, renderContext, phenomenon))
{
   auto& handler = AlertLayerHandler::Instance();

   for (auto alertActive : {false, true})
   {
      AddDrawItem(
         handler.SharedGeoLines(phenomenon, alertActive, renderContext));
   }
}

AlertLayer::~AlertLayer() = default;

void AlertLayer::InitializeHandler()
{
   static bool ftt = true;

   if (ftt)
   {
      logger_->debug("Initializing handler");
      AlertLayerHandler::Instance();
      ftt = false;
   }
}

std::size_t AlertLayer::SharedGeometrySegmentCount()
{
   return AlertLayerHandler::Instance().SharedGeoLineCount();
}

void AlertLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Initialize(mapContext);

   auto& alertLayerHandler = AlertLayerHandler::Instance();

   p->selectedTime_ = manager::TimelineManager::Instance()->GetSelectedTime();

   // Take a shared lock to prevent handling additional alerts while populating
   // initial lists
   std::shared_lock lock {alertLayerHandler.alertMutex_};

   if (alertLayerHandler.TryBeginPopulate(p->phenomenon_))
   {
      p->suppressNeedsRendering_ = true;
      for (auto alertActive : {false, true})
      {
         p->PopulateLines(alertActive);
      }
      p->suppressNeedsRendering_ = false;
      alertLayerHandler.EndPopulate(p->phenomenon_);
   }

   p->ConnectAlertHandlerSignals();
   AlertLayerHandler::Instance().RegisterGeometryBuilder(p->phenomenon_, this);
   Q_EMIT NeedsRendering();
}

void AlertLayer::Render(const std::shared_ptr<MapContext>& mapContext,
                        const QMapLibre::CustomLayerRenderParameters& params)
{
   auto& handler = AlertLayerHandler::Instance();
   for (auto alertActive : {false, true})
   {
      handler.SharedGeoLines(p->phenomenon_, alertActive, p->renderContext_)
         ->set_selected_time(p->selectedTime_);
   }

   DrawLayer::Render(mapContext, params);
}

#if defined(SCWX_RENDER_BACKEND_VULKAN)
void AlertLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto& handler = AlertLayerHandler::Instance();
   for (auto alertActive : {false, true})
   {
      handler.SharedGeoLines(p->phenomenon_, alertActive, p->renderContext_)
         ->set_selected_time(p->selectedTime_);
   }

   DrawLayer::RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
}
#endif

void AlertLayer::Deinitialize()
{
   logger_->debug("Deinitialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   AlertLayerHandler::Instance().UnregisterGeometryBuilder(p->phenomenon_,
                                                           this);

   DrawLayer::Deinitialize();
}

bool IsAlertActive(const std::shared_ptr<const awips::Segment>& segment)
{
   auto& vtec        = segment->header_->vtecString_.front();
   auto  action      = vtec.pVtec_.action();
   bool  alertActive = (action != awips::PVtec::Action::Canceled);

   return alertActive;
}

void AlertLayerHandler::HandleAlert(const types::TextEventKey& key,
                                    size_t                     messageIndex,
                                    boost::uuids::uuid         uuid)
{
   logger_->trace("HandleAlert: {}", key.ToString());

   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      alertsUpdated {};

   const auto& messageList = textEventManager_->message_list(key);

   // Find message by UUID instead of index, as the message index could have
   // changed between the signal being emitted and the handler being called
   auto messageIt = std::find_if(messageList.cbegin(),
                                 messageList.cend(),
                                 [&uuid](const auto& message)
                                 { return uuid == message->uuid(); });

   if (messageIt == messageList.cend())
   {
      logger_->warn(
         "Could not find alert uuid: {} ({})", key.ToString(), messageIndex);
      return;
   }

   auto& message       = *messageIt;
   auto  nextMessageIt = std::next(messageIt);

   // Store the current message index
   messageIndex =
      static_cast<std::size_t>(std::distance(messageList.cbegin(), messageIt));

   // Determine start time for first segment
   std::chrono::system_clock::time_point segmentBegin {};
   if (message->segment_count() > 0)
   {
      segmentBegin = message->segment(0)->event_begin();
   }

   // Determine the start time for the first segment of the next message
   std::optional<std::chrono::system_clock::time_point> nextMessageBegin {};
   if (nextMessageIt != messageList.cend())
   {
      nextMessageBegin =
         (*nextMessageIt)
            ->wmo_header()
            ->GetDateTime((*nextMessageIt)->segment(0)->event_begin());
   }

   // Take a unique mutex before modifying segments
   std::unique_lock lock {alertMutex_};

   // Update any existing earlier segments with new end time
   auto& segmentsForKey = segmentsByKey_[key];
   for (auto& segmentRecord : segmentsForKey)
   {
      // Determine if the segment is earlier than the current message
      auto it = std::find(
         messageList.cbegin(), messageList.cend(), segmentRecord->message_);
      auto segmentIndex =
         static_cast<std::size_t>(std::distance(messageList.cbegin(), it));

      if (segmentIndex < messageIndex &&
          segmentRecord->segmentEnd_ > segmentBegin)
      {
         segmentRecord->segmentEnd_ = segmentBegin;

         {
            std::lock_guard builderLock {geometryBuildersMutex_};
            const auto      builderIt =
               geometryBuilders_.find(segmentRecord->key_.phenomenon_);
            if (builderIt != geometryBuilders_.cend() &&
                builderIt->second != nullptr)
            {
               builderIt->second->p->UpdateAlert(segmentRecord);
            }
         }

         Q_EMIT AlertUpdated(segmentRecord);
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

      auto&             vtec        = segment->header_->vtecString_.front();
      awips::Phenomenon phenomenon  = vtec.pVtec_.phenomenon();
      bool              alertActive = IsAlertActive(segment);

      auto& segmentsForType = segmentsByType_[{key.phenomenon_, alertActive}];

      // Insert segment into lists
      std::shared_ptr<SegmentRecord> segmentRecord =
         std::make_shared<SegmentRecord>(segment, key, message);

      // Update segment end time to be no later than the begin time of the next
      // message (if present)
      if (nextMessageBegin.has_value() &&
          segmentRecord->segmentEnd_ > nextMessageBegin)
      {
         segmentRecord->segmentEnd_ = nextMessageBegin.value();
      }

      segmentsForKey.push_back(segmentRecord);
      segmentsForType.push_back(segmentRecord);

      {
         std::lock_guard builderLock {geometryBuildersMutex_};
         const auto      builderIt = geometryBuilders_.find(phenomenon);
         if (builderIt != geometryBuilders_.cend() &&
             builderIt->second != nullptr)
         {
            builderIt->second->p->AddAlert(segmentRecord);
         }
      }

      Q_EMIT AlertAdded(segmentRecord, phenomenon);

      alertsUpdated.emplace(phenomenon, alertActive);
   }

   // Release the lock after completing segment updates
   lock.unlock();

   for (auto& alert : alertsUpdated)
   {
      // Emit signal for each updated alert type
      Q_EMIT AlertsUpdated(alert.first, alert.second);

      std::lock_guard builderLock {geometryBuildersMutex_};
      const auto      builderIt = geometryBuilders_.find(alert.first);
      if (builderIt != geometryBuilders_.cend() && builderIt->second != nullptr)
      {
         if (InBulkLoad())
         {
            std::lock_guard bulkLock {bulkFinishMutex_};
            bulkFinishPending_.emplace(alert);
         }
         else
         {
            builderIt->second->p->FinishGeoLines(alert.second);
         }
      }
   }
}

void AlertLayerHandler::HandleAlertsRemoved(
   const std::unordered_set<types::TextEventKey,
                            types::TextEventHash<types::TextEventKey>>& keys)
{
   logger_->trace("HandleAlertsRemoved: {} keys", keys.size());

   std::set<awips::Phenomenon> alertsRemoved {};

   // Take a unique lock before modifying segments
   std::unique_lock lock {alertMutex_};

   for (const auto& key : keys)
   {
      // Remove segments associated with the key
      auto segmentsIt = segmentsByKey_.find(key);
      if (segmentsIt != segmentsByKey_.end())
      {
         for (const auto& segmentRecord : segmentsIt->second)
         {
            auto&      segment     = segmentRecord->segment_;
            const bool alertActive = IsAlertActive(segment);

            // Remove from segmentsByType_
            auto typeIt = segmentsByType_.find({key.phenomenon_, alertActive});
            if (typeIt != segmentsByType_.end())
            {
               auto& segmentsForType = typeIt->second;
               segmentsForType.erase(std::remove(segmentsForType.begin(),
                                                 segmentsForType.end(),
                                                 segmentRecord),
                                     segmentsForType.end());

               // If no segments remain for this type, erase the entry
               if (segmentsForType.empty())
               {
                  segmentsByType_.erase(typeIt);
               }
            }

            alertsRemoved.emplace(key.phenomenon_);
         }

         // Remove the key from segmentsByKey_
         segmentsByKey_.erase(segmentsIt);
      }
   }

   // Release the lock after completing segment updates
   lock.unlock();

   // Emit signal to notify that alerts have been removed
   for (auto& alert : alertsRemoved)
   {
      {
         std::lock_guard builderLock {geometryBuildersMutex_};
         const auto      builderIt = geometryBuilders_.find(alert);
         if (builderIt != geometryBuilders_.cend() &&
             builderIt->second != nullptr)
         {
            AlertLayer* layer = builderIt->second;
            QMetaObject::invokeMethod(
               layer,
               [layer]() { layer->p->RepopulateLines(); },
               Qt::QueuedConnection);
         }
      }

      Q_EMIT AlertsRemoved(alert);
   }
}

void AlertLayer::Impl::ConnectAlertHandlerSignals()
{
   if (handlerSignalsConnected_)
   {
      return;
   }
   handlerSignalsConnected_ = true;

   auto& alertLayerHandler = AlertLayerHandler::Instance();

   QObject::connect(&alertLayerHandler,
                    &AlertLayerHandler::AlertsNeedRendering,
                    receiver_.get(),
                    [this](awips::Phenomenon phenomenon)
                    {
                       if (phenomenon == phenomenon_)
                       {
                          Q_EMIT self_->NeedsRendering();
                       }
                    });
}

void AlertLayer::Impl::ConnectSignals()
{
   auto& alertPaletteSettings =
      settings::PaletteSettings::Instance().alert_palette(phenomenon_);
   auto timelineManager = manager::TimelineManager::Instance();

   QObject::connect(timelineManager.get(),
                    &manager::TimelineManager::SelectedTimeUpdated,
                    receiver_.get(),
                    [this](std::chrono::system_clock::time_point dateTime)
                    { selectedTime_ = dateTime; });

   connections_.push_back(alertPaletteSettings.changed_signal().connect(
      [this]()
      {
         UpdateLineData();
         UpdateLines();
         AlertLayerHandler::Instance().QueueAlertsNeedRendering(phenomenon_);
      }));
}

AlertLayerHandler::SharedPhenomenonGeometry& AlertLayer::Impl::SharedGeometry()
{
   return AlertLayerHandler::Instance().SharedGeometry(phenomenon_,
                                                       renderContext_);
}

void AlertLayerHandler::ScheduleRefresh()
{
   using namespace std::chrono_literals;

   std::unique_lock lock(refreshMutex_);

   const std::chrono::system_clock::time_point now =
      std::chrono::floor<std::chrono::minutes>(
         std::chrono::system_clock::now());
   refreshTimer_.expires_at(now + 1min);

   refreshTimer_.async_wait(
      [this](const boost::system::error_code& e)
      {
         if (e == boost::asio::error::operation_aborted)
         {
            logger_->debug("Alert refresh timer cancelled");
         }
         else if (e != boost::system::errc::success)
         {
            logger_->warn("Alert refresh timer error: {}", e.message());
         }
         else
         {
            QMetaObject::invokeMethod(
               this,
               [this]()
               {
                  if (!refreshEnabled_)
                  {
                     return;
                  }

                  std::lock_guard geometryLock {geometryMutex_};
                  for (const auto& [phenomenon, geometry] : geometry_)
                  {
                     (void) geometry;
                     QueueAlertsNeedRendering(phenomenon);
                  }

                  ScheduleRefresh();
               },
               Qt::QueuedConnection);
         }
      });
}

void AlertLayerHandler::BeginBulkLoad()
{
   bulkLoadDepth_.fetch_add(1, std::memory_order_acq_rel);
}

void AlertLayerHandler::EndBulkLoad()
{
   if (bulkLoadDepth_.fetch_sub(1, std::memory_order_acq_rel) > 1)
   {
      return;
   }

   std::unordered_set<std::pair<awips::Phenomenon, bool>,
                      AlertTypeHash<std::pair<awips::Phenomenon, bool>>>
      finishPending {};
   {
      std::lock_guard bulkLock {bulkFinishMutex_};
      finishPending.swap(bulkFinishPending_);
   }

   for (const auto& alert : finishPending)
   {
      std::lock_guard builderLock {geometryBuildersMutex_};
      const auto      builderIt = geometryBuilders_.find(alert.first);
      if (builderIt != geometryBuilders_.cend() && builderIt->second != nullptr)
      {
         builderIt->second->p->FinishGeoLines(alert.second);
      }
   }

   {
      std::lock_guard builderLock {geometryBuildersMutex_};
      for (const auto& builder : geometryBuilders_)
      {
         if (builder.second != nullptr)
         {
            QueueAlertsNeedRendering(builder.first);
         }
      }
   }

   renderCoalesceTimer_.stop();
   FlushQueuedAlertsNeedRendering();
}

void AlertLayerHandler::QueueAlertsNeedRendering(awips::Phenomenon phenomenon)
{
   {
      std::lock_guard lock {renderCoalesceMutex_};
      renderCoalescePending_.insert(phenomenon);
   }

   if (!renderCoalesceTimer_.isActive())
   {
      renderCoalesceTimer_.start();
   }
}

void AlertLayerHandler::FlushQueuedAlertsNeedRendering()
{
   std::unordered_set<awips::Phenomenon> pending {};
   {
      std::lock_guard lock {renderCoalesceMutex_};
      pending.swap(renderCoalescePending_);
   }

   for (const awips::Phenomenon phenomenon : pending)
   {
      Q_EMIT AlertsNeedRendering(phenomenon);
   }
}

AlertLayerHandler::SharedPhenomenonGeometry& AlertLayerHandler::SharedGeometry(
   awips::Phenomenon                             phenomenon,
   const std::shared_ptr<render::RenderContext>& renderContext)
{
   std::lock_guard lock {geometryMutex_};

   auto& geometry = geometry_[phenomenon];
   if (geometry.phenomenon == awips::Phenomenon::Unknown &&
       phenomenon != awips::Phenomenon::Unknown)
   {
      geometry.phenomenon = phenomenon;
   }

   for (auto alertActive : {false, true})
   {
      if (geometry.geoLines_.find(alertActive) == geometry.geoLines_.cend())
      {
         geometry.geoLines_.emplace(
            alertActive, std::make_shared<gl::draw::GeoLines>(renderContext));
      }
   }

   return geometry;
}

std::shared_ptr<gl::draw::GeoLines> AlertLayerHandler::SharedGeoLines(
   awips::Phenomenon                             phenomenon,
   bool                                          alertActive,
   const std::shared_ptr<render::RenderContext>& renderContext)
{
   return SharedGeometry(phenomenon, renderContext).geoLines_.at(alertActive);
}

bool AlertLayerHandler::TryBeginPopulate(awips::Phenomenon phenomenon)
{
   std::lock_guard lock {geometryMutex_};
   auto&           geometry = geometry_[phenomenon];
   if (geometry.populated_)
   {
      return false;
   }
   geometry.populated_ = true;
   return true;
}

void AlertLayerHandler::EndPopulate(awips::Phenomenon /* phenomenon */) {}

bool AlertLayerHandler::TryTrackSegment(
   const std::shared_ptr<SegmentRecord>& segmentRecord)
{
   std::lock_guard lock {trackedSegmentsMutex_};
   return trackedSegments_.insert(segmentRecord).second;
}

void AlertLayerHandler::ClearTrackedSegments()
{
   std::lock_guard lock {trackedSegmentsMutex_};
   trackedSegments_.clear();
}

void AlertLayerHandler::RepopulateGeometry(awips::Phenomenon phenomenon)
{
   std::lock_guard builderLock {geometryBuildersMutex_};
   const auto      builderIt = geometryBuilders_.find(phenomenon);
   if (builderIt != geometryBuilders_.cend() && builderIt->second != nullptr)
   {
      builderIt->second->p->RepopulateLines();
   }
}

std::optional<types::TextEventKey> AlertLayerHandler::KeyForGeoLine(
   const std::shared_ptr<const gl::draw::GeoLineDrawItem>& line)
{
   std::lock_guard lock {geometryMutex_};
   for (auto& [phenomenon, geometry] : geometry_)
   {
      std::lock_guard linesLock {geometry.linesMutex_};
      const auto      it = geometry.segmentsByLine_.find(line);
      if (it != geometry.segmentsByLine_.cend())
      {
         return it->second->key_;
      }
      (void) phenomenon;
   }
   return std::nullopt;
}

void AlertLayerHandler::ShowGeoLineHoverTooltip(
   awips::Phenomenon                                       phenomenon,
   const std::shared_ptr<const gl::draw::GeoLineDrawItem>& line,
   const QPointF&                                          mouseGlobalPos)
{
   std::lock_guard lock {geometryMutex_};
   const auto      geometryIt = geometry_.find(phenomenon);
   if (geometryIt == geometry_.cend())
   {
      return;
   }

   auto&           geometry = geometryIt->second;
   std::lock_guard linesLock {geometry.linesMutex_};

   if (line != geometry.lastHoverLine_.lock())
   {
      const auto it = geometry.segmentsByLine_.find(line);
      if (it != geometry.segmentsByLine_.cend())
      {
         geometry.hoverTooltip_ =
            boost::algorithm::join(it->second->segment_->productContent_, "\n");
      }
      else
      {
         geometry.hoverTooltip_.clear();
      }

      geometry.lastHoverLine_ = line;
   }

   if (!geometry.hoverTooltip_.empty())
   {
      util::tooltip::Show(geometry.hoverTooltip_, mouseGlobalPos);
   }
}

std::size_t AlertLayerHandler::SharedGeoLineCount()
{
   std::size_t     count = 0;
   std::lock_guard lock {geometryMutex_};
   for (auto& [phenomenon, geometry] : geometry_)
   {
      std::lock_guard linesLock {geometry.linesMutex_};
      count += geometry.linesBySegment_.size();
      (void) phenomenon;
   }
   return count;
}

void AlertLayerHandler::RegisterGeometryBuilder(awips::Phenomenon phenomenon,
                                                AlertLayer*       layer)
{
   std::lock_guard lock {geometryBuildersMutex_};
   if (!geometryBuilders_.contains(phenomenon))
   {
      geometryBuilders_.emplace(phenomenon, layer);
   }
}

void AlertLayerHandler::UnregisterGeometryBuilder(awips::Phenomenon phenomenon,
                                                  AlertLayer*       layer)
{
   std::lock_guard lock {geometryBuildersMutex_};
   const auto      it = geometryBuilders_.find(phenomenon);
   if (it != geometryBuilders_.cend() && it->second == layer)
   {
      geometryBuilders_.erase(it);
   }
}

void AlertLayer::Impl::AddAlert(
   const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord)
{
   auto& handler = AlertLayerHandler::Instance();
   if (!handler.TryTrackSegment(segmentRecord))
   {
      return;
   }

   auto& segment = segmentRecord->segment_;

   bool  alertActive = IsAlertActive(segment);
   auto& startTime   = segmentRecord->segmentBegin_;
   auto& endTime     = segmentRecord->segmentEnd_;

   auto& lineData = GetLineData(segment, alertActive);
   auto& shared   = SharedGeometry();
   auto& geoLines = shared.geoLines_.at(alertActive);

   const auto& coordinates = segment->codedLocation_->coordinates();

   // Take a mutex before modifying lines by segment
   std::unique_lock lock {shared.linesMutex_};

   // Add draw items only if the segment has not already been added
   auto drawItems = shared.linesBySegment_.try_emplace(
      segmentRecord,
      boost::container::stable_vector<
         std::shared_ptr<gl::draw::GeoLineDrawItem>> {});

   // If draw items were added
   if (drawItems.second)
   {
      constexpr bool borderHover = true;

      AddStyledLines(geoLines,
                     coordinates,
                     lineData,
                     startTime,
                     endTime,
                     borderHover,
                     drawItems.first->second);

      for (auto& di : drawItems.first->second)
      {
         shared.segmentsByLine_.insert({di, segmentRecord});
      }
   }

   if (!suppressNeedsRendering_ && !handler.InBulkLoad())
   {
      handler.QueueAlertsNeedRendering(phenomenon_);
   }
}

void AlertLayer::Impl::FinishGeoLines(bool alertActive)
{
   SharedGeometry().geoLines_.at(alertActive)->FinishLines();
}

void AlertLayer::Impl::UpdateAlert(
   const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord)
{
   auto& shared = SharedGeometry();

   // Take a mutex before referencing lines iterators and stable vector
   std::unique_lock lock {shared.linesMutex_};

   auto it = shared.linesBySegment_.find(segmentRecord);
   if (it != shared.linesBySegment_.cend())
   {
      auto& segment     = segmentRecord->segment_;
      bool  alertActive = IsAlertActive(segment);

      auto& geoLines = shared.geoLines_.at(alertActive);

      auto& lines = it->second;
      for (auto& line : lines)
      {
         geoLines->SetLineStartTime(line, segmentRecord->segmentBegin_);
         geoLines->SetLineEndTime(line, segmentRecord->segmentEnd_);
      }
   }

   auto& handler = AlertLayerHandler::Instance();
   if (!handler.InBulkLoad())
   {
      handler.QueueAlertsNeedRendering(phenomenon_);
   }
}

void AlertLayer::Impl::AddStyledLine(
   std::shared_ptr<gl::draw::GeoLines>&        geoLines,
   std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
   const common::Coordinate&                   p1,
   const common::Coordinate&                   p2,
   const LineData&                             lineData,
   std::chrono::system_clock::time_point       startTime,
   std::chrono::system_clock::time_point       endTime,
   bool                                        enableHover)
{
   geoLines->SetLineLocation(di,
                             static_cast<float>(p1.latitude_),
                             static_cast<float>(p1.longitude_),
                             static_cast<float>(p2.latitude_),
                             static_cast<float>(p2.longitude_));

   const auto strokeHalfWidths =
      ComputeGeoStrokeHalfWidths(static_cast<float>(lineData.lineWidth_),
                                 static_cast<float>(lineData.highlightWidth_),
                                 static_cast<float>(lineData.borderWidth_));

   geoLines->SetLineStrokeStyle(di,
                                lineData.lineColor_,
                                lineData.highlightColor_,
                                lineData.borderColor_,
                                strokeHalfWidths.lineHalf_,
                                strokeHalfWidths.highlightHalf_,
                                strokeHalfWidths.borderHalf_);
   geoLines->SetLineStartTime(di, startTime);
   geoLines->SetLineEndTime(di, endTime);

   if (enableHover)
   {
      const awips::Phenomenon phenomenon = phenomenon_;
      geoLines->SetLineHoverCallback(
         di,
         [phenomenon](
            const std::shared_ptr<gl::draw::GeoLineDrawItem>& hoverLine,
            const QPointF&                                    mouseGlobalPos)
         {
            AlertLayerHandler::Instance().ShowGeoLineHoverTooltip(
               phenomenon, hoverLine, mouseGlobalPos);
         });

      const std::weak_ptr<gl::draw::GeoLineDrawItem> diWeak = di;
      gl::draw::GeoLines::RegisterEventHandler(
         di,
         [this, diWeak](QEvent* event) { HandleGeoLinesEvent(diWeak, event); });
   }
}

void AlertLayer::Impl::AddStyledLines(
   std::shared_ptr<gl::draw::GeoLines>&   geoLines,
   const std::vector<common::Coordinate>& coordinates,
   const LineData&                        lineData,
   std::chrono::system_clock::time_point  startTime,
   std::chrono::system_clock::time_point  endTime,
   bool                                   enableHover,
   boost::container::stable_vector<std::shared_ptr<gl::draw::GeoLineDrawItem>>&
      drawItems)
{
   for (std::size_t i = 0, j = 1; i < coordinates.size(); ++i, ++j)
   {
      if (j >= coordinates.size())
      {
         j = 0;

         if (coordinates[i] == coordinates[j])
         {
            break;
         }
      }

      auto di = geoLines->AddLine();
      AddStyledLine(geoLines,
                    di,
                    coordinates[i],
                    coordinates[j],
                    lineData,
                    startTime,
                    endTime,
                    enableHover);

      drawItems.push_back(di);
   }
}

void AlertLayer::Impl::AddLines(
   std::shared_ptr<gl::draw::GeoLines>&   geoLines,
   const std::vector<common::Coordinate>& coordinates,
   const boost::gil::rgba32f_pixel_t&     color,
   float                                  width,
   std::chrono::system_clock::time_point  startTime,
   std::chrono::system_clock::time_point  endTime,
   bool                                   enableHover,
   boost::container::stable_vector<std::shared_ptr<gl::draw::GeoLineDrawItem>>&
      drawItems)
{
   for (std::size_t i = 0, j = 1; i < coordinates.size(); ++i, ++j)
   {
      if (j >= coordinates.size())
      {
         j = 0;

         // Ignore repeated coordinates at the end
         if (coordinates[i] == coordinates[j])
         {
            break;
         }
      }

      auto di = geoLines->AddLine();
      AddLine(geoLines,
              di,
              coordinates[i],
              coordinates[j],
              color,
              width,
              startTime,
              endTime,
              enableHover);

      drawItems.push_back(di);
   }
}

void AlertLayer::Impl::AddLine(std::shared_ptr<gl::draw::GeoLines>& geoLines,
                               std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                               const common::Coordinate&                   p1,
                               const common::Coordinate&                   p2,
                               const boost::gil::rgba32f_pixel_t&    color,
                               float                                 width,
                               std::chrono::system_clock::time_point startTime,
                               std::chrono::system_clock::time_point endTime,
                               bool enableHover)
{
   geoLines->SetLineLocation(di,
                             static_cast<float>(p1.latitude_),
                             static_cast<float>(p1.longitude_),
                             static_cast<float>(p2.latitude_),
                             static_cast<float>(p2.longitude_));
   geoLines->SetLineModulate(di, color);
   geoLines->SetLineWidth(di, width);
   geoLines->SetLineStartTime(di, startTime);
   geoLines->SetLineEndTime(di, endTime);

   if (enableHover)
   {
      const awips::Phenomenon phenomenon = phenomenon_;
      geoLines->SetLineHoverCallback(
         di,
         [phenomenon](
            const std::shared_ptr<gl::draw::GeoLineDrawItem>& hoverLine,
            const QPointF&                                    mouseGlobalPos)
         {
            AlertLayerHandler::Instance().ShowGeoLineHoverTooltip(
               phenomenon, hoverLine, mouseGlobalPos);
         });

      const std::weak_ptr<gl::draw::GeoLineDrawItem> diWeak = di;
      gl::draw::GeoLines::RegisterEventHandler(
         di,
         [this, diWeak](QEvent* event) { HandleGeoLinesEvent(diWeak, event); });
   }
}

void AlertLayer::Impl::PopulateLines(bool alertActive)
{
   auto& alertLayerHandler = AlertLayerHandler::Instance();
   auto& shared            = SharedGeometry();
   auto& geoLines          = shared.geoLines_.at(alertActive);

   geoLines->StartLines();

   // Populate initial segments
   auto segmentsIt =
      alertLayerHandler.segmentsByType_.find({phenomenon_, alertActive});
   if (segmentsIt != alertLayerHandler.segmentsByType_.cend())
   {
      for (auto& segment : segmentsIt->second)
      {
         AddAlert(segment);
      }
   }

   geoLines->FinishLines();
}

void AlertLayer::Impl::RepopulateLines()
{
   auto& alertLayerHandler = AlertLayerHandler::Instance();
   auto& shared            = SharedGeometry();

   // Take a shared lock to prevent handling additional alerts while populating
   // initial lists
   const std::shared_lock alertLock {alertLayerHandler.alertMutex_};

   {
      std::unique_lock linesLock {shared.linesMutex_};
      shared.linesBySegment_.clear();
      shared.segmentsByLine_.clear();
      shared.lastHoverLine_.reset();
      shared.hoverTooltip_.clear();
   }
   AlertLayerHandler::Instance().ClearTrackedSegments();

   suppressNeedsRendering_ = true;
   for (auto alertActive : {false, true})
   {
      PopulateLines(alertActive);
   }
   suppressNeedsRendering_ = false;

   auto& handler = AlertLayerHandler::Instance();
   if (!handler.InBulkLoad())
   {
      handler.QueueAlertsNeedRendering(phenomenon_);
   }
}

void AlertLayer::Impl::UpdateLines()
{
   auto&            shared = SharedGeometry();
   std::unique_lock lock {shared.linesMutex_};

   for (auto& segmentLine : shared.linesBySegment_)
   {
      auto& segmentRecord    = segmentLine.first;
      auto& geoLineDrawItems = segmentLine.second;
      if (geoLineDrawItems.empty())
      {
         continue;
      }

      auto& segment     = segmentRecord->segment_;
      bool  alertActive = IsAlertActive(segment);
      auto& lineData    = GetLineData(segment, alertActive);
      auto& geoLines    = shared.geoLines_.at(alertActive);

      const auto strokeHalfWidths = ComputeGeoStrokeHalfWidths(
         static_cast<float>(lineData.lineWidth_),
         static_cast<float>(lineData.highlightWidth_),
         static_cast<float>(lineData.borderWidth_));

      for (auto& line : geoLineDrawItems)
      {
         geoLines->SetLineStrokeStyle(line,
                                      lineData.lineColor_,
                                      lineData.highlightColor_,
                                      lineData.borderColor_,
                                      strokeHalfWidths.lineHalf_,
                                      strokeHalfWidths.highlightHalf_,
                                      strokeHalfWidths.borderHalf_);
      }
   }

   lock.unlock();

   FinishGeoLines(false);
   FinishGeoLines(true);
}

void AlertLayer::Impl::HandleGeoLinesEvent(
   std::weak_ptr<gl::draw::GeoLineDrawItem> diWeak, QEvent* ev)
{
   const std::shared_ptr<gl::draw::GeoLineDrawItem> di = diWeak.lock();
   if (di == nullptr)
   {
      return;
   }

   switch (ev->type())
   {
   case QEvent::Type::MouseButtonRelease:
   {
      std::shared_ptr<const AlertLayerHandler::SegmentRecord> segmentRecord;
      {
         auto&           shared = SharedGeometry();
         std::lock_guard lock {shared.linesMutex_};
         const auto      it = shared.segmentsByLine_.find(di);
         if (it != shared.segmentsByLine_.cend())
         {
            segmentRecord = it->second;
         }
      }

      if (segmentRecord != nullptr)
      {
         logger_->debug("Selected alert: {}", segmentRecord->key_.ToString());
         Q_EMIT self_->AlertSelected(segmentRecord->key_);
      }
      break;
   }

   default:
      break;
   }
}

AlertLayer::Impl::LineData
AlertLayer::Impl::CreateLineData(const settings::LineSettings& lineSettings)
{
   static constexpr float kAlertAlphaScale = 0.72f;
   static constexpr float kAlertWidthScale = 1.5f;

   LineData data {
      .borderColor_ {lineSettings.GetBorderColorRgba32f()},
      .highlightColor_ {lineSettings.GetHighlightColorRgba32f()},
      .lineColor_ {lineSettings.GetLineColorRgba32f()},
      .borderWidth_ =
         static_cast<std::size_t>(lineSettings.border_width().GetValue()),
      .highlightWidth_ =
         static_cast<std::size_t>(lineSettings.highlight_width().GetValue()),
      .lineWidth_ =
         static_cast<std::size_t>(lineSettings.line_width().GetValue())};

   data.lineColor_[3] *= kAlertAlphaScale;
   data.highlightColor_[3] *= kAlertAlphaScale;
   data.borderColor_[3] *= kAlertAlphaScale;

   data.lineWidth_ = std::max<std::size_t>(
      2U,
      static_cast<std::size_t>(
         std::lround(static_cast<double>(data.lineWidth_) * kAlertWidthScale)));

   // Single-band stroke: palette highlight/border widths produce dashed look.
   data.highlightWidth_ = 0;
   data.borderWidth_    = 0;
   data.highlightColor_ = data.lineColor_;
   data.borderColor_    = data.lineColor_;

   return data;
}

void AlertLayer::Impl::UpdateLineData()
{
   auto& alertPalette =
      settings::PaletteSettings().Instance().alert_palette(phenomenon_);

   for (auto threatCategory : ibw_.threatCategories_)
   {
      auto& palette = alertPalette.threat_category(threatCategory);
      threatCategoryLineData_.insert_or_assign(threatCategory,
                                               CreateLineData(palette));
   }

   if (ibw_.hasObservedTag_)
   {
      observedLineData_ = CreateLineData(alertPalette.observed());
   }

   if (ibw_.hasTornadoPossibleTag_)
   {
      tornadoPossibleLineData_ =
         CreateLineData(alertPalette.tornado_possible());
   }

   inactiveLineData_ = CreateLineData(alertPalette.inactive());
}

AlertLayer::Impl::LineData& AlertLayer::Impl::GetLineData(
   const std::shared_ptr<const awips::Segment>& segment, bool alertActive)
{
   if (!alertActive)
   {
      return inactiveLineData_;
   }

   for (auto& threatCategory : ibw_.threatCategories_)
   {
      if (segment->threatCategory_ == threatCategory)
      {
         if (threatCategory == awips::ibw::ThreatCategory::Base)
         {
            if (ibw_.hasObservedTag_ && segment->observed_)
            {
               return observedLineData_;
            }

            if (ibw_.hasTornadoPossibleTag_ && segment->tornadoPossible_)
            {
               return tornadoPossibleLineData_;
            }
         }

         return threatCategoryLineData_.at(threatCategory);
      }
   }

   return threatCategoryLineData_.at(awips::ibw::ThreatCategory::Base);
};

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

} // namespace scwx::qt::map

#include "alert_layer.moc"
