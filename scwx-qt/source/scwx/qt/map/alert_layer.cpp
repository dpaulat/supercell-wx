#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/util/color.hpp>
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

static const boost::gil::rgba32f_pixel_t kBlack_ {0.0f, 0.0f, 0.0f, 1.0f};

template<class Key>
struct AlertTypeHash;

template<>
struct AlertTypeHash<std::pair<awips::Phenomenon, bool>>
{
   size_t operator()(const std::pair<awips::Phenomenon, bool>& x) const;
};

class AlertLayerHandler : public QObject
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
   void AlertAdded(const std::shared_ptr<SegmentRecord>& segmentRecord,
                   awips::Phenomenon                     phenomenon);
   void AlertUpdated(const std::shared_ptr<SegmentRecord>& segmentRecord);
   void AlertsUpdated(awips::Phenomenon phenomenon, bool alertActive);
};

class AlertLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<MapContext> context,
                 awips::Phenomenon           phenomenon) :
       phenomenon_ {phenomenon},
       lines_ {{false, std::make_shared<gl::draw::GeoLines>(context)},
               {true, std::make_shared<gl::draw::GeoLines>(context)}}
   {
      auto& paletteSettings = settings::PaletteSettings::Instance();

      for (auto alertActive : {false, true})
      {
         lineColor_.emplace(
            alertActive,
            util::color::ToRgba8PixelT(
               paletteSettings.alert_color(phenomenon_, alertActive)
                  .GetValue()));
      }

      ConnectSignals();
   }
   ~Impl() { receiver_ = nullptr; };

   void AddAlert(
      const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord);
   void UpdateAlert(
      const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord);
   void ConnectSignals();

   static void AddLine(std::shared_ptr<gl::draw::GeoLines>&        lines,
                       std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                       const common::Coordinate&                   p1,
                       const common::Coordinate&                   p2,
                       boost::gil::rgba32f_pixel_t                 color,
                       float                                       width,
                       std::chrono::system_clock::time_point       startTime,
                       std::chrono::system_clock::time_point       endTime);
   static void
   AddLines(std::shared_ptr<gl::draw::GeoLines>&   lines,
            const std::vector<common::Coordinate>& coordinates,
            boost::gil::rgba32_pixel_t             color,
            float                                  width,
            std::chrono::system_clock::time_point  startTime,
            std::chrono::system_clock::time_point  endTime,
            std::vector<std::shared_ptr<gl::draw::GeoLineDrawItem>>& drawItems);

   const awips::Phenomenon phenomenon_;

   std::unique_ptr<QObject> receiver_ {std::make_unique<QObject>()};

   std::unordered_map<bool, std::shared_ptr<gl::draw::GeoLines>> lines_;

   std::unordered_map<bool, boost::gil::rgba8_pixel_t> lineColor_;
};

AlertLayer::AlertLayer(std::shared_ptr<MapContext> context,
                       awips::Phenomenon           phenomenon) :
    DrawLayer(context), p(std::make_unique<Impl>(context, phenomenon))
{
   for (auto alertActive : {false, true})
   {
      auto& lines = p->lines_.at(alertActive);

      AddDrawItem(lines);
   }
}

AlertLayer::~AlertLayer() = default;

void AlertLayer::Initialize()
{
   logger_->debug("Initialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Initialize();

   for (auto alertActive : {false, true})
   {
      auto& lines = p->lines_.at(alertActive);

      lines->StartLines();
      lines->FinishLines();
   }
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

void AlertLayer::Impl::ConnectSignals()
{
   QObject::connect(
      &AlertLayerHandler::Instance(),
      &AlertLayerHandler::AlertAdded,
      receiver_.get(),
      [this](
         const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord,
         awips::Phenomenon                                        phenomenon)
      {
         if (phenomenon == phenomenon_)
         {
            AddAlert(segmentRecord);
         }
      });
}

void AlertLayer::Impl::AddAlert(
   const std::shared_ptr<AlertLayerHandler::SegmentRecord>& segmentRecord)
{
   auto& segment = segmentRecord->segment_;

   auto& vtec        = segment->header_->vtecString_.front();
   auto  action      = vtec.pVtec_.action();
   bool  alertActive = (action != awips::PVtec::Action::Canceled);
   auto& startTime   = segmentRecord->segmentBegin_;
   auto& endTime     = segmentRecord->segmentEnd_;

   auto& lineColor = lineColor_.at(alertActive);
   auto& lines     = lines_.at(alertActive);

   const auto& coordinates = segment->codedLocation_->coordinates();

   std::vector<std::shared_ptr<gl::draw::GeoLineDrawItem>> drawItems {};

   AddLines(lines, coordinates, kBlack_, 5.0f, startTime, endTime, drawItems);
   AddLines(lines, coordinates, lineColor, 3.0f, startTime, endTime, drawItems);

   lines->FinishLines();
}

void AlertLayer::Impl::UpdateAlert(
   [[maybe_unused]] const std::shared_ptr<AlertLayerHandler::SegmentRecord>&
      segmentRecord)
{
   // TODO
}

void AlertLayer::Impl::AddLines(
   std::shared_ptr<gl::draw::GeoLines>&                     lines,
   const std::vector<common::Coordinate>&                   coordinates,
   boost::gil::rgba32_pixel_t                               color,
   float                                                    width,
   std::chrono::system_clock::time_point                    startTime,
   std::chrono::system_clock::time_point                    endTime,
   std::vector<std::shared_ptr<gl::draw::GeoLineDrawItem>>& drawItems)
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

      auto di = lines->AddLine();
      AddLine(lines,
              di,
              coordinates[i],
              coordinates[j],
              color,
              width,
              startTime,
              endTime);

      drawItems.push_back(di);
   }
}

void AlertLayer::Impl::AddLine(std::shared_ptr<gl::draw::GeoLines>& lines,
                               std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                               const common::Coordinate&                   p1,
                               const common::Coordinate&                   p2,
                               boost::gil::rgba32f_pixel_t           color,
                               float                                 width,
                               std::chrono::system_clock::time_point startTime,
                               std::chrono::system_clock::time_point endTime)
{
   lines->SetLineLocation(
      di, p1.latitude_, p1.longitude_, p2.latitude_, p2.longitude_);
   lines->SetLineModulate(di, color);
   lines->SetLineWidth(di, width);
   lines->SetLineStartTime(di, startTime);
   lines->SetLineEndTime(di, endTime);
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
