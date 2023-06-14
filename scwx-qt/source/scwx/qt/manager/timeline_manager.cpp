#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>

#include <condition_variable>
#include <mutex>

#include <boost/asio/steady_timer.hpp>
#include <fmt/chrono.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::timeline_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

enum class Direction
{
   Back,
   Next
};

// Wait up to 5 seconds for radar sweeps to update
static constexpr std::chrono::seconds kRadarSweepMonitorTimeout_ {5};

class TimelineManager::Impl
{
public:
   explicit Impl(TimelineManager* self) : self_ {self} {}

   ~Impl()
   {
      // Lock mutexes before destroying
      std::unique_lock animationTimerLock {animationTimerMutex_};
      animationTimer_.cancel();

      std::unique_lock selectTimeLock {selectTimeMutex_};
   }

   TimelineManager* self_;

   std::pair<std::chrono::system_clock::time_point,
             std::chrono::system_clock::time_point>
        GetLoopStartAndEndTimes();
   void UpdateCacheLimit(
      std::shared_ptr<manager::RadarProductManager> radarProductManager,
      const std::set<std::chrono::system_clock::time_point>& volumeTimes);

   void RadarSweepMonitorDisable();
   void RadarSweepMonitorReset();
   void RadarSweepMonitorWait(std::unique_lock<std::mutex>& lock);

   void Pause();
   void Play();
   void
   SelectTimeAsync(std::chrono::system_clock::time_point selectedTime = {});
   std::pair<bool, bool>
        SelectTime(std::chrono::system_clock::time_point selectedTime = {});
   void StepAsync(Direction direction);

   std::size_t                           mapCount_ {0};
   std::string                           radarSite_ {"?"};
   std::string                           previousRadarSite_ {"?"};
   std::chrono::system_clock::time_point pinnedTime_ {};
   std::chrono::system_clock::time_point adjustedTime_ {};
   std::chrono::system_clock::time_point selectedTime_ {};
   types::MapTime                        viewType_ {types::MapTime::Live};
   std::chrono::minutes                  loopTime_ {30};
   double                                loopSpeed_ {5.0};
   std::chrono::milliseconds             loopDelay_ {2500};

   bool                    radarSweepMonitorActive_ {false};
   std::mutex              radarSweepMonitorMutex_ {};
   std::condition_variable radarSweepMonitorCondition_ {};
   std::set<std::size_t>   radarSweepsUpdated_ {};
   std::set<std::size_t>   radarSweepsComplete_ {};

   types::AnimationState     animationState_ {types::AnimationState::Pause};
   boost::asio::steady_timer animationTimer_ {scwx::util::io_context()};
   std::mutex                animationTimerMutex_ {};

   std::mutex selectTimeMutex_ {};
};

TimelineManager::TimelineManager() : p(std::make_unique<Impl>(this)) {}
TimelineManager::~TimelineManager() = default;

void TimelineManager::SetMapCount(std::size_t mapCount)
{
   p->mapCount_ = mapCount;
}

void TimelineManager::SetRadarSite(const std::string& radarSite)
{
   if (p->radarSite_ == radarSite)
   {
      // No action needed
      return;
   }

   logger_->debug("SetRadarSite: {}", radarSite);

   p->radarSite_ = radarSite;

   if (p->viewType_ == types::MapTime::Live)
   {
      // If the selected view type is live, select the current products
      p->SelectTime();
   }
   else
   {
      // If the selected view type is archive, select using the selected time
      p->SelectTimeAsync(p->selectedTime_);
   }
}

void TimelineManager::SetDateTime(
   std::chrono::system_clock::time_point dateTime)
{
   logger_->debug("SetDateTime: {}", scwx::util::TimeString(dateTime));

   p->pinnedTime_ = dateTime;

   if (p->viewType_ == types::MapTime::Archive)
   {
      // Only select if the view type is archive
      p->SelectTimeAsync(dateTime);
   }

   // Ignore a date/time selection if the view type is live
}

void TimelineManager::SetViewType(types::MapTime viewType)
{
   logger_->debug("SetViewType: {}", types::GetMapTimeName(viewType));

   p->viewType_ = viewType;

   if (p->viewType_ == types::MapTime::Live)
   {
      // If the selected view type is live, select the current products
      p->SelectTime();
   }
   else
   {
      // If the selected view type is archive, select using the pinned time
      p->SelectTimeAsync(p->pinnedTime_);
   }
}

void TimelineManager::SetLoopTime(std::chrono::minutes loopTime)
{
   logger_->debug("SetLoopTime: {}", loopTime);

   p->loopTime_ = loopTime;
}

void TimelineManager::SetLoopSpeed(double loopSpeed)
{
   logger_->debug("SetLoopSpeed: {}", loopSpeed);

   if (loopSpeed < 1.0)
   {
      loopSpeed = 1.0;
   }

   p->loopSpeed_ = loopSpeed;
}

void TimelineManager::SetLoopDelay(std::chrono::milliseconds loopDelay)
{
   logger_->debug("SetLoopDelay: {}", loopDelay);

   p->loopDelay_ = loopDelay;
}

void TimelineManager::AnimationStepBegin()
{
   logger_->debug("AnimationStepBegin");

   p->Pause();

   if (p->viewType_ == types::MapTime::Live ||
       p->pinnedTime_ == std::chrono::system_clock::time_point {})
   {
      // If the selected view type is live, select the current products
      p->SelectTimeAsync(std::chrono::system_clock::now() - p->loopTime_);
   }
   else
   {
      // If the selected view type is archive, select using the pinned time
      p->SelectTimeAsync(p->pinnedTime_ - p->loopTime_);
   }
}

void TimelineManager::AnimationStepBack()
{
   logger_->debug("AnimationStepBack");

   p->Pause();
   p->StepAsync(Direction::Back);
}

void TimelineManager::AnimationPlayPause()
{
   if (p->animationState_ == types::AnimationState::Pause)
   {
      logger_->debug("AnimationPlay");
      p->Play();
   }
   else
   {
      logger_->debug("AnimationPause");
      p->Pause();
   }
}

void TimelineManager::AnimationStepNext()
{
   logger_->debug("AnimationStepNext");

   p->Pause();
   p->StepAsync(Direction::Next);
}

void TimelineManager::AnimationStepEnd()
{
   logger_->debug("AnimationStepEnd");

   p->Pause();

   if (p->viewType_ == types::MapTime::Live)
   {
      // If the selected view type is live, select the current products
      p->SelectTime();
   }
   else
   {
      // If the selected view type is archive, select using the pinned time
      p->SelectTimeAsync(p->pinnedTime_);
   }
}

void TimelineManager::Impl::RadarSweepMonitorDisable()
{
   radarSweepMonitorActive_ = false;
}

void TimelineManager::Impl::RadarSweepMonitorReset()
{
   radarSweepsUpdated_.clear();
   radarSweepsComplete_.clear();

   radarSweepMonitorActive_ = true;
}

void TimelineManager::Impl::RadarSweepMonitorWait(
   std::unique_lock<std::mutex>& lock)
{
   radarSweepMonitorCondition_.wait_for(lock, kRadarSweepMonitorTimeout_);
   radarSweepMonitorActive_ = false;
}

void TimelineManager::ReceiveRadarSweepUpdated(std::size_t mapIndex)
{
   if (!p->radarSweepMonitorActive_)
   {
      return;
   }

   std::unique_lock lock {p->radarSweepMonitorMutex_};

   // Radar sweep is updated, but still needs painted
   p->radarSweepsUpdated_.insert(mapIndex);
}

void TimelineManager::ReceiveRadarSweepNotUpdated(
   std::size_t mapIndex, types::NoUpdateReason /* reason */)
{
   if (!p->radarSweepMonitorActive_)
   {
      return;
   }

   std::unique_lock lock {p->radarSweepMonitorMutex_};

   // Radar sweep is complete, no painting will occur
   p->radarSweepsComplete_.insert(mapIndex);

   // If all sweeps have completed rendering
   if (p->radarSweepsComplete_.size() == p->mapCount_)
   {
      // Notify monitors
      p->radarSweepMonitorActive_ = false;
      p->radarSweepMonitorCondition_.notify_all();
   }
}

void TimelineManager::ReceiveMapWidgetPainted(std::size_t mapIndex)
{
   if (!p->radarSweepMonitorActive_)
   {
      return;
   }

   std::unique_lock lock {p->radarSweepMonitorMutex_};

   // If the radar sweep has been updated
   if (p->radarSweepsUpdated_.contains(mapIndex))
   {
      // Mark the radar sweep complete
      p->radarSweepsUpdated_.erase(mapIndex);
      p->radarSweepsComplete_.insert(mapIndex);

      // If all sweeps have completed rendering
      if (p->radarSweepsComplete_.size() == p->mapCount_)
      {
         // Notify monitors
         p->radarSweepMonitorActive_ = false;
         p->radarSweepMonitorCondition_.notify_all();
      }
   }
}

void TimelineManager::Impl::Pause()
{
   // Cancel animation
   std::unique_lock animationTimerLock {animationTimerMutex_};
   animationTimer_.cancel();

   if (animationState_ != types::AnimationState::Pause)
   {
      animationState_ = types::AnimationState::Pause;
      Q_EMIT self_->AnimationStateUpdated(animationState_);
   }
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
TimelineManager::Impl::GetLoopStartAndEndTimes()
{
   // Determine loop end time
   std::chrono::system_clock::time_point endTime;

   if (viewType_ == types::MapTime::Live ||
       pinnedTime_ == std::chrono::system_clock::time_point {})
   {
      endTime = std::chrono::floor<std::chrono::minutes>(
         std::chrono::system_clock::now());
   }
   else
   {
      endTime = pinnedTime_;
   }

   // Determine loop start time and current position in the loop
   std::chrono::system_clock::time_point startTime = endTime - loopTime_;

   return {startTime, endTime};
}

void TimelineManager::Impl::UpdateCacheLimit(
   std::shared_ptr<manager::RadarProductManager>          radarProductManager,
   const std::set<std::chrono::system_clock::time_point>& volumeTimes)
{
   // Calculate the number of volume scans in the loop
   auto [startTime, endTime] = GetLoopStartAndEndTimes();
   auto startIter = util::GetBoundedElementIterator(volumeTimes, startTime);
   auto endIter   = util::GetBoundedElementIterator(volumeTimes, endTime);
   std::size_t numVolumeScans = std::distance(startIter, endIter) + 1;

   // Dynamically update maximum cached volume scans to 1.5x the loop length
   radarProductManager->SetCacheLimit(
      static_cast<std::size_t>(numVolumeScans * 1.5));
}

void TimelineManager::Impl::Play()
{
   using namespace std::chrono_literals;

   if (animationState_ != types::AnimationState::Play)
   {
      animationState_ = types::AnimationState::Play;
      Q_EMIT self_->AnimationStateUpdated(animationState_);
   }

   {
      std::unique_lock animationTimerLock {animationTimerMutex_};
      animationTimer_.cancel();
   }

   scwx::util::async(
      [this]()
      {
         // Take a lock for time selection
         std::unique_lock lock {selectTimeMutex_};

         auto [startTime, endTime] = GetLoopStartAndEndTimes();
         std::chrono::system_clock::time_point currentTime = selectedTime_;
         std::chrono::system_clock::time_point newTime;

         if (currentTime < startTime || currentTime >= endTime)
         {
            // If the currently selected time is out of the loop, select the
            // start time
            newTime = startTime;
         }
         else
         {
            // If the currently selected time is in the loop, increment
            newTime = currentTime + 1min;
         }

         // Unlock prior to selecting time
         lock.unlock();

         // Lock radar sweep monitor
         std::unique_lock radarSweepMonitorLock {radarSweepMonitorMutex_};

         // Reset radar sweep monitor in preparation for update
         RadarSweepMonitorReset();

         // Select the time
         auto selectTimeStart = std::chrono::steady_clock::now();
         auto [volumeTimeUpdated, selectedTimeUpdated] = SelectTime(newTime);
         auto selectTimeEnd = std::chrono::steady_clock::now();
         auto elapsedTime   = selectTimeEnd - selectTimeStart;

         if (volumeTimeUpdated)
         {
            // Wait for radar sweeps to update
            RadarSweepMonitorWait(radarSweepMonitorLock);
         }
         else
         {
            // Disable radar sweep monitor
            RadarSweepMonitorDisable();
         }

         // Calculate the interval until the next update, prior to selecting
         std::chrono::milliseconds interval;
         if (newTime != endTime)
         {
            // Determine repeat interval (speed of 1.0 is 1 minute per second)
            interval = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::milliseconds(std::lroundl(1000.0 / loopSpeed_)) -
               elapsedTime);
         }
         else
         {
            // Pause at the end of the loop
            interval = std::chrono::duration_cast<std::chrono::milliseconds>(
               loopDelay_ - elapsedTime);
         }

         std::unique_lock animationTimerLock {animationTimerMutex_};
         animationTimer_.expires_after(interval);
         animationTimer_.async_wait(
            [this](const boost::system::error_code& e)
            {
               if (e == boost::system::errc::success)
               {
                  if (animationState_ == types::AnimationState::Play)
                  {
                     Play();
                  }
               }
               else if (e == boost::asio::error::operation_aborted)
               {
                  logger_->debug("Play timer cancelled");
               }
               else
               {
                  logger_->warn("Play timer error: {}", e.message());
               }
            });
      });
}

void TimelineManager::Impl::SelectTimeAsync(
   std::chrono::system_clock::time_point selectedTime)
{
   scwx::util::async([=, this]() { SelectTime(selectedTime); });
}

std::pair<bool, bool> TimelineManager::Impl::SelectTime(
   std::chrono::system_clock::time_point selectedTime)
{
   bool volumeTimeUpdated   = false;
   bool selectedTimeUpdated = false;

   if (selectedTime_ == selectedTime && radarSite_ == previousRadarSite_)
   {
      // Nothing to do
      return {volumeTimeUpdated, selectedTimeUpdated};
   }
   else if (selectedTime == std::chrono::system_clock::time_point {})
   {
      // If a default time point is given, reset to a live view
      selectedTime_      = selectedTime;
      adjustedTime_      = selectedTime;
      previousRadarSite_ = radarSite_;

      logger_->debug("Time updated: Live");

      Q_EMIT self_->LiveStateUpdated(true);
      Q_EMIT self_->VolumeTimeUpdated(selectedTime);
      Q_EMIT self_->SelectedTimeUpdated(selectedTime);

      volumeTimeUpdated   = true;
      selectedTimeUpdated = true;

      return {volumeTimeUpdated, selectedTimeUpdated};
   }

   // Take a lock for time selection
   std::unique_lock lock {selectTimeMutex_};

   // Request active volume times
   auto radarProductManager =
      manager::RadarProductManager::Instance(radarSite_);
   auto volumeTimes = radarProductManager->GetActiveVolumeTimes(selectedTime);

   // Dynamically update maximum cached volume scans
   UpdateCacheLimit(radarProductManager, volumeTimes);

   // Find the best match bounded time
   auto elementPtr = util::GetBoundedElementPointer(volumeTimes, selectedTime);

   // The timeline is no longer live
   Q_EMIT self_->LiveStateUpdated(false);

   if (elementPtr != nullptr)
   {
      // If the adjusted time changed, or if a new radar site has been selected
      if (adjustedTime_ != *elementPtr || radarSite_ != previousRadarSite_)
      {
         // If the time was found, select it
         adjustedTime_ = *elementPtr;

         logger_->debug("Volume time updated: {}",
                        scwx::util::TimeString(adjustedTime_));

         volumeTimeUpdated = true;
         Q_EMIT self_->VolumeTimeUpdated(adjustedTime_);
      }
   }
   else
   {
      // No volume time was found
      logger_->info("No volume scan found for {}",
                    scwx::util::TimeString(selectedTime));
   }

   logger_->trace("Selected time updated: {}",
                  scwx::util::TimeString(selectedTime));

   selectedTime_       = selectedTime;
   selectedTimeUpdated = true;
   Q_EMIT self_->SelectedTimeUpdated(selectedTime);

   previousRadarSite_ = radarSite_;

   return {volumeTimeUpdated, selectedTimeUpdated};
}

void TimelineManager::Impl::StepAsync(Direction direction)
{
   scwx::util::async(
      [=, this]()
      {
         // Take a lock for time selection
         std::unique_lock lock {selectTimeMutex_};

         // Determine time to get active volume times
         std::chrono::system_clock::time_point queryTime = adjustedTime_;
         if (queryTime == std::chrono::system_clock::time_point {})
         {
            queryTime = std::chrono::system_clock::now();
         }

         // Request active volume times
         auto radarProductManager =
            manager::RadarProductManager::Instance(radarSite_);
         auto volumeTimes =
            radarProductManager->GetActiveVolumeTimes(queryTime);

         if (volumeTimes.empty())
         {
            logger_->debug("No products to step through");
            return;
         }

         // Dynamically update maximum cached volume scans
         UpdateCacheLimit(radarProductManager, volumeTimes);

         std::set<std::chrono::system_clock::time_point>::const_iterator it;

         if (adjustedTime_ == std::chrono::system_clock::time_point {})
         {
            // If the adjusted time is live, get the last element in the set
            it = std::prev(volumeTimes.cend());
         }
         else
         {
            // Get the current element in the set
            it = scwx::util::GetBoundedElementIterator(volumeTimes,
                                                       adjustedTime_);
         }

         if (it == volumeTimes.cend())
         {
            // Should not get here, but protect against an error
            logger_->error("No suitable volume time found");
            return;
         }

         if (direction == Direction::Back)
         {
            // Only if we aren't at the beginning of the volume times set
            if (it != volumeTimes.cbegin())
            {
               // Select the previous time
               adjustedTime_ = *(--it);
               selectedTime_ = adjustedTime_;

               logger_->debug("Volume time updated: {}",
                              scwx::util::TimeString(adjustedTime_));

               Q_EMIT self_->LiveStateUpdated(false);
               Q_EMIT self_->VolumeTimeUpdated(adjustedTime_);
               Q_EMIT self_->SelectedTimeUpdated(adjustedTime_);
            }
         }
         else
         {
            // Only if we aren't at the end of the volume times set
            if (it != std::prev(volumeTimes.cend()))
            {
               // Select the next time
               adjustedTime_ = *(++it);
               selectedTime_ = adjustedTime_;

               logger_->debug("Volume time updated: {}",
                              scwx::util::TimeString(adjustedTime_));

               Q_EMIT self_->LiveStateUpdated(false);
               Q_EMIT self_->VolumeTimeUpdated(adjustedTime_);
               Q_EMIT self_->SelectedTimeUpdated(adjustedTime_);
            }
         }
      });
}

std::shared_ptr<TimelineManager> TimelineManager::Instance()
{
   static std::weak_ptr<TimelineManager> timelineManagerReference_ {};
   static std::mutex                     instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<TimelineManager> timelineManager =
      timelineManagerReference_.lock();

   if (timelineManager == nullptr)
   {
      timelineManager           = std::make_shared<TimelineManager>();
      timelineManagerReference_ = timelineManager;
   }

   return timelineManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
