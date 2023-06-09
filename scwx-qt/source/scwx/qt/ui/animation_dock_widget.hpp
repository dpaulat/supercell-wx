#pragma once

#include <scwx/qt/types/map_types.hpp>

#include <chrono>

#include <QDockWidget>

namespace Ui
{
class AnimationDockWidget;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class AnimationDockWidgetImpl;

class AnimationDockWidget : public QDockWidget
{
   Q_OBJECT

public:
   explicit AnimationDockWidget(QWidget* parent = nullptr);
   ~AnimationDockWidget();

public slots:
   void UpdateAnimationState(types::AnimationState state);
   void UpdateLiveState(bool isLive);

signals:
   void ViewTypeChanged(types::MapTime viewType);
   void DateTimeChanged(std::chrono::system_clock::time_point dateTime);

   void LoopTimeChanged(std::chrono::minutes loopTime);
   void LoopSpeedChanged(double loopSpeed);
   void LoopDelayChanged(std::chrono::milliseconds loopDelay);

   void AnimationStepBeginSelected();
   void AnimationStepBackSelected();
   void AnimationPlaySelected();
   void AnimationStepNextSelected();
   void AnimationStepEndSelected();

private:
   friend class AnimationDockWidgetImpl;
   std::unique_ptr<AnimationDockWidgetImpl> p;
   Ui::AnimationDockWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
