#include "animation_dock_widget.hpp"
#include "ui_animation_dock_widget.h"

#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::animation_dock_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AnimationDockWidgetImpl
{
public:
   explicit AnimationDockWidgetImpl() = default;
   ~AnimationDockWidgetImpl()         = default;
};

AnimationDockWidget::AnimationDockWidget(QWidget* parent) :
    QDockWidget(parent),
    p {std::make_unique<AnimationDockWidgetImpl>()},
    ui(new Ui::AnimationDockWidget)
{
   ui->setupUi(this);
}

AnimationDockWidget::~AnimationDockWidget()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
