#pragma once

#include <scwx/qt/types/map_types.hpp>

#include <chrono>
#include <memory>

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

   void SetMapCount(std::size_t mapCount);

public slots:
   void SetRadarSite(const std::string& radarSite);

   void SetDateTime(std::chrono::system_clock::time_point dateTime);
   void SetViewType(types::MapTime viewType);

   void SetLoopTime(std::chrono::minutes loopTime);
   void SetLoopSpeed(double loopSpeed);

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
