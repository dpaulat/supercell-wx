#include "map_popout_frame.hpp"

#include <scwx/qt/map/map_widget.hpp>

#include <QCloseEvent>
#include <QVBoxLayout>

namespace scwx::qt::main
{

MapPopoutFrame::MapPopoutFrame(std::size_t mapIndex, QWidget* parent) :
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): parented to this
    QWidget(parent), mapIndex_ {mapIndex}, vbox_ {new QVBoxLayout(this)}
{
   setWindowFlags(Qt::Window);
   setAttribute(Qt::WA_DeleteOnClose, false);
   setWindowTitle(tr("Map %1").arg(static_cast<int>(mapIndex) + 1));
   vbox_->setContentsMargins(0, 0, 0, 0);
   vbox_->setSpacing(0);
}

void MapPopoutFrame::SetEmbeddedMap(scwx::qt::map::MapWidget* map)
{
   if (map_ != nullptr)
   {
      vbox_->removeWidget(map_);
      map_->setParent(nullptr);
   }
   map_ = map;
   if (map_ != nullptr)
   {
      vbox_->addWidget(map_);
      map_->setVisible(true);
   }
}

void MapPopoutFrame::DetachMapWidget()
{
   if (map_ == nullptr)
   {
      return;
   }
   vbox_->removeWidget(map_);
   // removeWidget() does not un-parent; if we keep map_ as a child,
   // ~MapPopoutFrame deletes the MapWidget and the host's maps_ entry becomes
   // dangling.
   map_->setParent(nullptr);
   map_ = nullptr;
}

void MapPopoutFrame::closeEvent(QCloseEvent* event)
{
   event->ignore();
   hide();
   Q_EMIT DockMapRequested();
}

} // namespace scwx::qt::main
