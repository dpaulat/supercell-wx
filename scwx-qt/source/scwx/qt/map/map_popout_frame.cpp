#include <scwx/qt/map/map_popout_frame.hpp>
#include <scwx/qt/map/map_widget.hpp>

#include <QCloseEvent>
#include <QVBoxLayout>

namespace scwx::qt::map
{

class MapPopoutFrame::Impl
{
public:
   explicit Impl(MapPopoutFrame* self, std::size_t mapIndex) :
       mapIndex_ {mapIndex},
       // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
       vbox_ {new QVBoxLayout(self)}
   {
      vbox_->setContentsMargins(0, 0, 0, 0);
      vbox_->setSpacing(0);
   }

   void SetEmbeddedMap(MapWidget* map)
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

   void DetachMapWidget()
   {
      if (map_ == nullptr)
      {
         return;
      }
      vbox_->removeWidget(map_);
      // removeWidget() does not un-parent; if we keep map_ as a child,
      // ~MapPopoutFrame deletes the MapWidget and the host's maps_ entry
      // becomes dangling.
      map_->setParent(nullptr);
      map_ = nullptr;
   }

   std::size_t  mapIndex_ {};
   QVBoxLayout* vbox_ {};
   MapWidget*   map_ {nullptr};
};

MapPopoutFrame::MapPopoutFrame(std::size_t mapIndex, QWidget* parent) :
    QWidget {parent}, p_ {std::make_unique<Impl>(this, mapIndex)}
{
   setWindowFlags(Qt::Window);
   setAttribute(Qt::WA_DeleteOnClose, false);
   setWindowTitle(tr("Map %1").arg(static_cast<int>(p_->mapIndex_) + 1));
}

MapPopoutFrame::~MapPopoutFrame() = default;

void MapPopoutFrame::SetEmbeddedMap(MapWidget* map)
{
   p_->SetEmbeddedMap(map);
}

void MapPopoutFrame::DetachMapWidget()
{
   p_->DetachMapWidget();
}

std::size_t MapPopoutFrame::map_index() const
{
   return p_->mapIndex_;
}

void MapPopoutFrame::closeEvent(QCloseEvent* event)
{
   event->ignore();
   hide();
   Q_EMIT DockMapRequested();
}

} // namespace scwx::qt::map
