#pragma once

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

private:
   friend class AnimationDockWidgetImpl;
   std::unique_ptr<AnimationDockWidgetImpl> p;
   Ui::AnimationDockWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
