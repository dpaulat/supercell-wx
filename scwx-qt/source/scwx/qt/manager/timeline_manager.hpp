#pragma once

#include <scwx/qt/types/map_types.hpp>

#include <chrono>
#include <memory>
#include <utility>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class TimelineManager : public QObject
{
   Q_OBJECT

public:
   explicit TimelineManager();
   ~TimelineManager();

   static std::shared_ptr<TimelineManager> Instance();

   [[nodiscard]] std::chrono::system_clock::time_point GetSelectedTime() const;
   [[nodiscard]] types::MapTime                        GetViewType() const;

   // The current loop's [start, end) range -- end is "now" (live) or the
   // pinned archive time, start is end minus the configured loop duration.
   // Exposes Impl::GetLoopStartAndEndTimes(), already used internally by
   // AnimationStepBegin/UpdateCacheLimit, to external consumers that need
   // to know the loop's range without duplicating live-vs-pinned-time
   // logic (e.g. a data layer prefetching everything the loop will need).
   [[nodiscard]] std::pair<std::chrono::system_clock::time_point,
                           std::chrono::system_clock::time_point>
   GetLoopStartAndEndTimes() const;

   void SetMapCount(std::size_t mapCount);

public slots:
   void SetRadarSite(const std::string& radarSite);

   void SetDateTime(std::chrono::system_clock::time_point dateTime);
   void SetViewType(types::MapTime viewType);

   void SetLoopTime(std::chrono::minutes loopTime);
   void SetLoopSpeed(double loopSpeed);
   void SetLoopDelay(std::chrono::milliseconds loopDelay);

   void AnimationStepBegin();
   void AnimationStepBack();
   void AnimationPlayPause();
   void AnimationStepNext();
   void AnimationStepEnd();

   void ReceiveRadarSweepUpdated(std::size_t mapIndex);
   void ReceiveRadarSweepNotUpdated(std::size_t           mapIndex,
                                    types::NoUpdateReason reason);
   void ReceiveMapWidgetPainted(std::size_t mapIndex);

signals:
   void SelectedTimeUpdated(std::chrono::system_clock::time_point dateTime);
   void VolumeTimeUpdated(std::chrono::system_clock::time_point dateTime);

   void AnimationStateUpdated(types::AnimationState state);
   void LiveStateUpdated(bool isLive);
   void ViewTypeUpdated(types::MapTime viewType);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
