#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/map.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>

#include <mutex>

#include <fmt/chrono.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::timeline_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class TimelineManager::Impl
{
public:
   explicit Impl(TimelineManager* self) : self_ {self} {}

   ~Impl()
   {
      // Lock mutexes before destroying
      std::unique_lock selectTimeLock {selectTimeMutex_};
   }

   TimelineManager* self_;

   void SelectTime(std::chrono::system_clock::time_point selectedTime = {});

   std::string                           radarSite_ {"?"};
   std::string                           previousRadarSite_ {"?"};
   std::chrono::system_clock::time_point pinnedTime_ {};
   std::chrono::system_clock::time_point adjustedTime_ {};
   std::chrono::system_clock::time_point selectedTime_ {};
   types::MapTime                        viewType_ {types::MapTime::Live};
   std::chrono::minutes                  loopTime_ {30};
   double                                loopSpeed_ {1.0};

   std::mutex selectTimeMutex_ {};
};

TimelineManager::TimelineManager() : p(std::make_unique<Impl>(this)) {}
TimelineManager::~TimelineManager() = default;

void TimelineManager::SetRadarSite(const std::string& radarSite)
{
   p->radarSite_ = radarSite;

   if (p->viewType_ == types::MapTime::Live)
   {
      // If the selected view type is live, select the current products
      p->SelectTime();
   }
   else
   {
      // If the selected view type is archive, select using the selected time
      p->SelectTime(p->selectedTime_);
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
      p->SelectTime(dateTime);
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
      p->SelectTime(p->pinnedTime_);
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

   p->loopSpeed_ = loopSpeed;
}

void TimelineManager::AnimationStepBegin()
{
   logger_->debug("AnimationStepBegin");
}

void TimelineManager::AnimationStepBack()
{
   logger_->debug("AnimationStepBack");
}

void TimelineManager::AnimationPlay()
{
   logger_->debug("AnimationPlay");
}

void TimelineManager::AnimationPause()
{
   logger_->debug("AnimationPause");
}

void TimelineManager::AnimationStepNext()
{
   logger_->debug("AnimationStepNext");
}

void TimelineManager::AnimationStepEnd()
{
   logger_->debug("AnimationStepEnd");
}

void TimelineManager::Impl::SelectTime(
   std::chrono::system_clock::time_point selectedTime)
{
   if (selectedTime_ == selectedTime && radarSite_ == previousRadarSite_)
   {
      // Nothing to do
      return;
   }
   else if (selectedTime == std::chrono::system_clock::time_point {})
   {
      // If a default time point is given, reset to a live view
      selectedTime_ = selectedTime;
      adjustedTime_ = selectedTime;

      logger_->debug("Time updated: Live");

      emit self_->TimeUpdated(selectedTime);

      return;
   }

   scwx::util::async(
      [=, this]()
      {
         // Take a lock for time selection
         std::unique_lock lock {selectTimeMutex_};

         // Request active volume times
         auto radarProductManager =
            manager::RadarProductManager::Instance(radarSite_);
         auto volumeTimes =
            radarProductManager->GetActiveVolumeTimes(selectedTime);

         // Find the best match bounded time
         auto elementPtr =
            util::GetBoundedElementPointer(volumeTimes, selectedTime);

         if (elementPtr != nullptr)
         {
            selectedTime_ = selectedTime;

            // If the adjusted time changed, or if a new radar site has been
            // selected
            if (adjustedTime_ != *elementPtr ||
                radarSite_ != previousRadarSite_)
            {
               // If the time was found, select it
               adjustedTime_ = *elementPtr;

               logger_->debug("Time updated: {}", adjustedTime_);

               emit self_->TimeUpdated(adjustedTime_);
            }
         }
         else
         {
            // No volume time was found
            logger_->info("No volume scan found for {}", selectedTime);
         }

         previousRadarSite_ = radarSite_;
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
