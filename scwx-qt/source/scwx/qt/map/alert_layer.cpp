#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
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
      connect(textEventManager_.get(),
              &manager::TextEventManager::AlertUpdated,
              this,
              &AlertLayerHandler::HandleAlert);
      connect(textEventManager_.get(),
              &manager::TextEventManager::AlertsRemoved,
              this,
              &AlertLayerHandler::HandleAlertsRemoved);
   }
   ~AlertLayerHandler()
   {
      disconnect(textEventManager_.get(), nullptr, this, nullptr);

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

   std::shared_ptr<manager::TextEventManager> textEventManager_ {
      manager::TextEventManager::Instance()};

   std::shared_mutex alertMutex_ {};

signals:
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

   explicit Impl(AlertLayer*                           self,
                 const std::shared_ptr<gl::GlContext>& glContext,
                 awips::Phenomenon                     phenomenon) :
       self_ {self},
       phenomenon_ {phenomenon},
       ibw_ {awips::ibw::GetImpactBasedWarningInfo(phenomenon)},
       geoLines_ {{false, std::make_shared<gl::draw::GeoLines>(glContext)},
                  {true, std::make_shared<gl::draw::GeoLines>(glContext)}}
   {
      UpdateLineData();
      ConnectSignals();
      ScheduleRefresh();
   }
   ~Impl()
   {
      std::unique_lock refreshLock(refreshMutex_);
      refreshEnabled_ = false;
      refreshTimer_.cancel();
      refreshLock.unlock();

      threadPool_.stop();
      threadPool_.join();

      receiver_ = nullptr;

      std::unique_lock lock(linesMutex_);
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
   void HandleGeoLinesEvent(std::weak_ptr<gl::draw::GeoLineDrawItem>& di,
                            QEvent*                                   ev);
   void
   HandleGeoLinesHover(const std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                       const QPointF& mouseGlobalPos);
   void ScheduleRefresh();

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

   static LineData CreateLineData(const settings::LineSettings& lineSettings);

   boost::asio::thread_pool threadPool_ {1u};

   AlertLayer* self_;

   std::atomic<bool>         refreshEnabled_ {true};
   boost::asio::system_timer refreshTimer_ {threadPool_};
   std::mutex                refreshMutex_;

   const awips::Phenomenon                   phenomenon_;
   const awips::ibw::ImpactBasedWarningInfo& ibw_;

   std::unique_ptr<QObject> receiver_ {std::make_unique<QObject>()};
   std::mutex               receiverMutex_ {};

   std::unordered_map<bool, std::shared_ptr<gl::draw::GeoLines>> geoLines_;

   std::unordered_map<std::shared_ptr<const AlertLayerHandler::SegmentRecord>,
                      boost::container::stable_vector<
                         std::shared_ptr<gl::draw::GeoLineDrawItem>>>
      linesBySegment_ {};
   std::unordered_map<std::shared_ptr<const gl::draw::GeoLineDrawItem>,
                      std::shared_ptr<const AlertLayerHandler::SegmentRecord>>
              segmentsByLine_;
   std::mutex linesMutex_ {};

   std::unordered_map<awips::ibw::ThreatCategory, LineData>
            threatCategoryLineData_;
   LineData observedLineData_ {};
   LineData tornadoPossibleLineData_ {};
   LineData inactiveLineData_ {};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::shared_ptr<const gl::draw::GeoLineDrawItem> lastHoverDi_ {nullptr};
   std::string                                      tooltip_ {};

   std::vector<boost::signals2::scoped_connection> connections_ {};
};

AlertLayer::AlertLayer(const std::shared_ptr<gl::GlContext>& glContext,
                       awips::Phenomenon                     phenomenon) :
    DrawLayer(
       glContext,
       fmt::format("AlertLayer {}", awips::GetPhenomenonText(phenomenon))),
    p(std::make_unique<Impl>(this, glContext, phenomenon))
{
   for (auto alertActive : {false, true})
   {
      auto& geoLines = p->geoLines_.at(alertActive);

      AddDrawItem(geoLines);
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

void AlertLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Initialize(mapContext);

   auto& alertLayerHandler = AlertLayerHandler::Instance();

   p->selectedTime_ = manager::TimelineManager::Instance()->GetSelectedTime();

   // Take a shared lock to prevent handling additional alerts while populating
   // initial lists
   std::shared_lock lock {alertLayerHandler.alertMutex_};

   for (auto alertActive : {false, true})
   {
      p->PopulateLines(alertActive);
   }

   p->ConnectAlertHandlerSignals();
}

void AlertLayer::Render(const std::shared_ptr<MapContext>& mapContext,
                        const QMapLibre::CustomLayerRenderParameters& params)
{
   for (auto alertActive : {false, true})
   {
      p->geoLines_.at(alertActive)->set_selected_time(p->selectedTime_);
   }

   DrawLayer::Render(mapContext, params);

   SCWX_GL_CHECK_ERROR();
}

void AlertLayer::Deinitialize()
{
   logger_->debug("Deinitialize: {}", awips::GetPhenomenonText(p->phenomenon_));

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

      Q_EMIT AlertAdded(segmentRecord, phenomenon);

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
      Q_EMIT AlertsRemoved(alert);
   }
}

void AlertLayer::Impl::ConnectAlertHandlerSignals()
{
   auto& alertLayerHandler = AlertLayerHandler::Instance();

   QObject::connect(
      &alertLayerHandler,
      &AlertLayerHandler::AlertAdded,
      receiver_.get(),
      [this](
         const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord,
         awips::Phenomenon                                        phenomenon)
      {
         if (phenomenon == phenomenon_)
         {
            // Only process one signal at a time
            const std::unique_lock lock {receiverMutex_};

            AddAlert(segmentRecord);
         }
      });
   QObject::connect(
      &alertLayerHandler,
      &AlertLayerHandler::AlertUpdated,
      receiver_.get(),
      [this](
         const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord)
      {
         if (segmentRecord->key_.phenomenon_ == phenomenon_)
         {
            // Only process one signal at a time
            const std::unique_lock lock {receiverMutex_};

            UpdateAlert(segmentRecord);
         }
      });
   QObject::connect(&alertLayerHandler,
                    &AlertLayerHandler::AlertsRemoved,
                    receiver_.get(),
                    [this](awips::Phenomenon phenomenon)
                    {
                       if (phenomenon == phenomenon_)
                       {
                          // Only process one signal at a time
                          const std::unique_lock lock {receiverMutex_};

                          // Re-populate the lines if multiple alerts were
                          // removed
                          RepopulateLines();
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
      }));
}

void AlertLayer::Impl::ScheduleRefresh()
{
   using namespace std::chrono_literals;

   // Take a unique lock before refreshing
   std::unique_lock lock(refreshMutex_);

   // Expires at the top of the next minute
   std::chrono::system_clock::time_point now =
      std::chrono::floor<std::chrono::minutes>(
         std::chrono::system_clock::now());
   refreshTimer_.expires_at(now + 1min);

   refreshTimer_.async_wait(
      [this](const boost::system::error_code& e)
      {
         if (e == boost::asio::error::operation_aborted)
         {
            logger_->debug("Refresh timer cancelled");
         }
         else if (e != boost::system::errc::success)
         {
            logger_->warn("Refresh timer error: {}", e.message());
         }
         else
         {
            Q_EMIT self_->NeedsRendering();

            if (refreshEnabled_)
            {
               ScheduleRefresh();
            }
         }
      });
}

void AlertLayer::Impl::AddAlert(
   const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord)
{
   auto& segment = segmentRecord->segment_;

   bool  alertActive = IsAlertActive(segment);
   auto& startTime   = segmentRecord->segmentBegin_;
   auto& endTime     = segmentRecord->segmentEnd_;

   auto& lineData = GetLineData(segment, alertActive);
   auto& geoLines = geoLines_.at(alertActive);

   const auto& coordinates = segment->codedLocation_->coordinates();

   // Take a mutex before modifying lines by segment
   std::unique_lock lock {linesMutex_};

   // Add draw items only if the segment has not already been added
   auto drawItems = linesBySegment_.try_emplace(
      segmentRecord,
      boost::container::stable_vector<
         std::shared_ptr<gl::draw::GeoLineDrawItem>> {});

   // If draw items were added
   if (drawItems.second)
   {
      const auto borderWidth    = static_cast<float>(lineData.borderWidth_);
      const auto highlightWidth = static_cast<float>(lineData.highlightWidth_);
      const auto lineWidth      = static_cast<float>(lineData.lineWidth_);

      const float totalHighlightWidth = lineWidth + (highlightWidth * 2.0f);
      const float totalBorderWidth = totalHighlightWidth + (borderWidth * 2.0f);

      constexpr bool borderHover    = true;
      constexpr bool highlightHover = false;
      constexpr bool lineHover      = false;

      // Add border
      AddLines(geoLines,
               coordinates,
               lineData.borderColor_,
               totalBorderWidth,
               startTime,
               endTime,
               borderHover,
               drawItems.first->second);

      // Add border to segmentsByLine_
      for (auto& di : drawItems.first->second)
      {
         segmentsByLine_.insert({di, segmentRecord});
      }

      // Add highlight
      AddLines(geoLines,
               coordinates,
               lineData.highlightColor_,
               totalHighlightWidth,
               startTime,
               endTime,
               highlightHover,
               drawItems.first->second);

      // Add line
      AddLines(geoLines,
               coordinates,
               lineData.lineColor_,
               lineWidth,
               startTime,
               endTime,
               lineHover,
               drawItems.first->second);
   }

   Q_EMIT self_->NeedsRendering();
}

void AlertLayer::Impl::UpdateAlert(
   const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord)
{
   // Take a mutex before referencing lines iterators and stable vector
   std::unique_lock lock {linesMutex_};

   auto it = linesBySegment_.find(segmentRecord);
   if (it != linesBySegment_.cend())
   {
      auto& segment     = segmentRecord->segment_;
      bool  alertActive = IsAlertActive(segment);

      auto& geoLines = geoLines_.at(alertActive);

      auto& lines = it->second;
      for (auto& line : lines)
      {
         geoLines->SetLineStartTime(line, segmentRecord->segmentBegin_);
         geoLines->SetLineEndTime(line, segmentRecord->segmentEnd_);
      }
   }

   Q_EMIT self_->NeedsRendering();
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
      geoLines->SetLineHoverCallback(
         di,
         std::bind(&AlertLayer::Impl::HandleGeoLinesHover,
                   this,
                   std::placeholders::_1,
                   std::placeholders::_2));

      const std::weak_ptr<gl::draw::GeoLineDrawItem> diWeak = di;
      gl::draw::GeoLines::RegisterEventHandler(
         di,
         std::bind(&AlertLayer::Impl::HandleGeoLinesEvent,
                   this,
                   diWeak,
                   std::placeholders::_1));
   }
}

void AlertLayer::Impl::PopulateLines(bool alertActive)
{
   auto& alertLayerHandler = AlertLayerHandler::Instance();
   auto& geoLines          = geoLines_.at(alertActive);

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

   // Take a shared lock to prevent handling additional alerts while populating
   // initial lists
   const std::shared_lock alertLock {alertLayerHandler.alertMutex_};

   linesBySegment_.clear();
   segmentsByLine_.clear();

   for (auto alertActive : {false, true})
   {
      PopulateLines(alertActive);
   }

   Q_EMIT self_->NeedsRendering();
}

void AlertLayer::Impl::UpdateLines()
{
   std::unique_lock lock {linesMutex_};

   for (auto& segmentLine : linesBySegment_)
   {
      auto& segmentRecord    = segmentLine.first;
      auto& geoLineDrawItems = segmentLine.second;
      auto& segment          = segmentRecord->segment_;
      bool  alertActive      = IsAlertActive(segment);
      auto& lineData         = GetLineData(segment, alertActive);
      auto& geoLines         = geoLines_.at(alertActive);

      const auto borderWidth    = static_cast<float>(lineData.borderWidth_);
      const auto highlightWidth = static_cast<float>(lineData.highlightWidth_);
      const auto lineWidth      = static_cast<float>(lineData.lineWidth_);

      const float totalHighlightWidth = lineWidth + (highlightWidth * 2.0f);
      const float totalBorderWidth = totalHighlightWidth + (borderWidth * 2.0f);

      // Border, highlight and line
      std::size_t linesPerType = geoLineDrawItems.size() / 3;

      // Border
      for (auto& borderLine : geoLineDrawItems | std::views::take(linesPerType))
      {
         geoLines->SetLineModulate(borderLine, lineData.borderColor_);
         geoLines->SetLineWidth(borderLine, totalBorderWidth);
      }

      // Highlight
      for (auto& highlightLine : geoLineDrawItems |
                                    std::views::drop(linesPerType) |
                                    std::views::take(linesPerType))
      {
         geoLines->SetLineModulate(highlightLine, lineData.highlightColor_);
         geoLines->SetLineWidth(highlightLine, totalHighlightWidth);
      }

      // Line
      for (auto& line : geoLineDrawItems | std::views::drop(linesPerType * 2))
      {
         geoLines->SetLineModulate(line, lineData.lineColor_);
         geoLines->SetLineWidth(line, lineWidth);
      }
   }
}

void AlertLayer::Impl::HandleGeoLinesEvent(
   std::weak_ptr<gl::draw::GeoLineDrawItem>& diWeak, QEvent* ev)
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
      auto it = segmentsByLine_.find(di);
      if (it != segmentsByLine_.cend())
      {
         // Display alert dialog
         logger_->debug("Selected alert: {}", it->second->key_.ToString());
         Q_EMIT self_->AlertSelected(it->second->key_);
      }
      break;
   }

   default:
      break;
   }
}

void AlertLayer::Impl::HandleGeoLinesHover(
   const std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
   const QPointF&                                    mouseGlobalPos)
{
   if (di != lastHoverDi_)
   {
      auto it = segmentsByLine_.find(di);
      if (it != segmentsByLine_.cend())
      {
         tooltip_ =
            boost::algorithm::join(it->second->segment_->productContent_, "\n");
      }
      else
      {
         tooltip_.clear();
      }

      lastHoverDi_ = di;
   }

   if (!tooltip_.empty())
   {
      util::tooltip::Show(tooltip_, mouseGlobalPos);
   }
}

AlertLayer::Impl::LineData
AlertLayer::Impl::CreateLineData(const settings::LineSettings& lineSettings)
{
   return LineData {
      .borderColor_ {lineSettings.GetBorderColorRgba32f()},
      .highlightColor_ {lineSettings.GetHighlightColorRgba32f()},
      .lineColor_ {lineSettings.GetLineColorRgba32f()},
      .borderWidth_ =
         static_cast<std::size_t>(lineSettings.border_width().GetValue()),
      .highlightWidth_ =
         static_cast<std::size_t>(lineSettings.highlight_width().GetValue()),
      .lineWidth_ =
         static_cast<std::size_t>(lineSettings.line_width().GetValue())};
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
