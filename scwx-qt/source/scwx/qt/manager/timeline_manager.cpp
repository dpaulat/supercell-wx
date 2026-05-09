#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/util/queue_counter.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/time.hpp>

#include <condition_variable>
#include <mutex>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
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
// Only allow for 3 steps to be queued at any time
static constexpr size_t kMaxQueuedSteps_ {3};

class TimelineManager::Impl
{
public:
   explicit Impl(TimelineManager* self) : self_ {self}
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      loopDelay_ =
         std::chrono::milliseconds(generalSettings.loop_delay().GetValue());
      loopSpeed_ = generalSettings.loop_speed().GetValue();
      loopTime_  = std::chrono::minutes(generalSettings.loop_time().GetValue());
   }

   ~Impl()
   {
      // Lock mutexes before destroying
      std::unique_lock animationTimerLock {animationTimerMutex_};
      animationTimer_.cancel();
      animationTimerLock.unlock();

      selectThreadPool_.stop();
      playThreadPool_.stop();

      selectThreadPool_.join();
      playThreadPool_.join();
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
   void PlaySync();
   void
   SelectTimeAsync(std::chrono::system_clock::time_point selectedTime = {});
   std::pair<bool, bool>
        SelectTime(std::chrono::system_clock::time_point selectedTime = {});
   void StepAsync(Direction direction);
   void Step(Direction direction);

   boost::asio::thread_pool playThreadPool_ {1};
   boost::asio::thread_pool selectThreadPool_ {1};

   util::QueueCounter stepCounter_ {kMaxQueuedSteps_};

   std::size_t                           mapCount_ {0};
   std::string                           radarSite_ {"?"};
   std::string                           previousRadarSite_ {"?"};
   std::chrono::system_clock::time_point pinnedTime_ {};
   std::chrono::system_clock::time_point adjustedTime_ {};
   std::chrono::system_clock::time_point selectedTime_ {};
   types::MapTime                        viewType_ {types::MapTime::Live};
   std::chrono::minutes                  loopTime_;
   double                                loopSpeed_;
   std::chrono::milliseconds             loopDelay_;

   bool                    radarSweepMonitorActive_ {false};
   std::mutex              radarSweepMonitorMutex_ {};
   std::condition_variable radarSweepMonitorCondition_ {};
   std::set<std::size_t>   radarSweepsUpdated_ {};
   std::set<std::size_t>   radarSweepsComplete_ {};

   types::AnimationState     animationState_ {types::AnimationState::Pause};
   boost::asio::steady_timer animationTimer_ {playThreadPool_};
   std::mutex                animationTimerMutex_ {};

   std::mutex selectTimeMutex_ {};
};

TimelineManager::TimelineManager() : p(std::make_unique<Impl>(this)) {}
TimelineManager::~TimelineManager() = default;

std::chrono::system_clock::time_point TimelineManager::GetSelectedTime() const
{
   return p->selectedTime_;
}

types::MapTime TimelineManager::GetViewType() const
{
   return p->viewType_;
}

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
      // If the selected view type is archive, select using the pinned time.
      // A default time of {} is treated as "live" by SelectTime; when first
      // switching to archive the dock may not have emitted a date yet, so
      // pin to the current wall-clock time (minute resolution) so the UI
      // shows archive time instead of "Live".
      if (p->pinnedTime_ == std::chrono::system_clock::time_point {})
      {
         p->pinnedTime_ =
            std::chrono::floor<std::chrono::minutes>(scwx::util::time::now());
      }
      p->SelectTimeAsync(p->pinnedTime_);
   }

   Q_EMIT ViewTypeUpdated(viewType);
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
      p->SelectTimeAsync(scwx::util::time::now() - p->loopTime_);
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
      p->SelectTimeAsync();
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
   std::cv_status status =
      radarSweepMonitorCondition_.wait_for(lock, kRadarSweepMonitorTimeout_);
   if (status == std::cv_status::timeout)
   {
      logger_->debug("Radar sweep monitor timed out");
   }
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

void TimelineManager::ReceiveRadarSweepNotUpdated(std::size_t mapIndex,
                                                  types::NoUpdateReason reason)
{
   if (!p->radarSweepMonitorActive_ ||
       reason == types::NoUpdateReason::NotLoaded)
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
   if (p->radarSweepsUpdated_.contains(mapIndex) &&
       !p->radarSweepsComplete_.contains(mapIndex))
   {
      // Mark the radar sweep complete
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
      endTime =
         std::chrono::floor<std::chrono::minutes>(scwx::util::time::now());
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
   auto startIter =
      scwx::util::GetBoundedElementIterator(volumeTimes, startTime);
   auto endIter = scwx::util::GetBoundedElementIterator(volumeTimes, endTime);
   std::size_t numVolumeScans = std::distance(startIter, endIter) + 1;

   // Dynamically update maximum cached volume scans to the lesser of
   // either 1.5x the loop length or 5 greater than the loop length
   radarProductManager->SetCacheLimit(std::min(
      static_cast<std::size_t>(numVolumeScans * 1.5), numVolumeScans + 5u));
}

void TimelineManager::Impl::Play()
{
   if (animationState_ != types::AnimationState::Play)
   {
      animationState_ = types::AnimationState::Play;
      Q_EMIT self_->AnimationStateUpdated(animationState_);
   }

   {
      std::unique_lock animationTimerLock {animationTimerMutex_};
      animationTimer_.cancel();
   }

   boost::asio::post(playThreadPool_,
                     [this]()
                     {
                        try
                        {
                           PlaySync();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void TimelineManager::Impl::PlaySync()
{
   using namespace std::chrono_literals;

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

   auto selectTimeStart = std::chrono::steady_clock::now();
   if (radarSite_.empty())
   {
      // No radar product: sweeps will not complete the monitor; skip the wait
      // so play advances at the configured loop rate instead of timing out.
      SelectTime(newTime);
   }
   else
   {
      // Lock radar sweep monitor
      std::unique_lock radarSweepMonitorLock {radarSweepMonitorMutex_};

      // Reset radar sweep monitor in preparation for update
      RadarSweepMonitorReset();

      // Select the time
      SelectTime(newTime);

      // Wait for radar sweeps to update
      RadarSweepMonitorWait(radarSweepMonitorLock);
   }
   auto selectTimeEnd = std::chrono::steady_clock::now();
   auto elapsedTime   = selectTimeEnd - selectTimeStart;

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
}

void TimelineManager::Impl::SelectTimeAsync(
   std::chrono::system_clock::time_point selectedTime)
{
   boost::asio::post(selectThreadPool_,
                     [=, this]()
                     {
                        try
                        {
                           SelectTime(selectedTime);
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
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
      std::unique_lock const lock {selectTimeMutex_};

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

   if (radarSite_.empty())
   {
      std::unique_lock const lock {selectTimeMutex_};

      adjustedTime_      = selectedTime;
      selectedTime_      = selectedTime;
      previousRadarSite_ = radarSite_;

      Q_EMIT self_->LiveStateUpdated(selectedTime ==
                                     std::chrono::system_clock::time_point {});
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
   auto elementPtr =
      scwx::util::GetBoundedElementPointer(volumeTimes, selectedTime);

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
   // Prevent too many steps from being added to the queue
   if (!stepCounter_.add())
   {
      return;
   }

   boost::asio::post(selectThreadPool_,
                     [=, this]()
                     {
                        try
                        {
                           Step(direction);
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                        stepCounter_.remove();
                     });
}

void TimelineManager::Impl::Step(Direction direction)
{
   // Take a lock for time selection
   std::unique_lock lock {selectTimeMutex_};

   std::chrono::system_clock::time_point newTime = selectedTime_;

   if (newTime == std::chrono::system_clock::time_point {})
   {
      if (direction == Direction::Back)
      {
         newTime =
            std::chrono::floor<std::chrono::minutes>(scwx::util::time::now());
      }
      else
      {
         // Cannot step forward any further
         return;
      }
   }

   // Unlock prior to selecting time
   lock.unlock();

   if (radarSite_.empty())
   {
      // No radar: apply a single one-minute step without waiting for sweeps
      // that will never be recorded as complete.
      using namespace std::chrono_literals;
      if (direction == Direction::Back)
      {
         newTime -= 1min;
      }
      else
      {
         newTime += 1min;
         if (newTime > scwx::util::time::now() + 2min)
         {
            return;
         }
      }
      SelectTime(newTime);
      return;
   }

   // Lock radar sweep monitor
   std::unique_lock radarSweepMonitorLock {radarSweepMonitorMutex_};

   // Attempt to step forward or backward up to 30 minutes until an update is
   // received on at least one map
   for (std::size_t i = 0; i < 30; ++i)
   {
      using namespace std::chrono_literals;

      // Increment/decrement selected time by one minute
      if (direction == Direction::Back)
      {
         newTime -= 1min;
      }
      else
      {
         newTime += 1min;

         // If the new time is more than 2 minutes in the future, stop stepping
         if (newTime > scwx::util::time::now() + 2min)
         {
            break;
         }
      }

      // Reset radar sweep monitor in preparation for update
      RadarSweepMonitorReset();

      // Select the time
      SelectTime(newTime);

      // Wait for radar sweeps to update
      RadarSweepMonitorWait(radarSweepMonitorLock);

      // Check for updates
      if (!radarSweepsUpdated_.empty())
      {
         break;
      }
   }
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
