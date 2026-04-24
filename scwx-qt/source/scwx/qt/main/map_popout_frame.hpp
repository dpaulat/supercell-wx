#pragma once

#include <QVBoxLayout>
#include <QWidget>

#include <cstddef>

class QCloseEvent;
namespace scwx::qt::map
{
class MapWidget;
}

namespace scwx::qt::main
{

// Borderless top-level window holding a single map pane (no main menu, no
// docks).
class MapPopoutFrame : public QWidget
{
   Q_OBJECT

public:
   explicit MapPopoutFrame(std::size_t mapIndex, QWidget* parent = nullptr);
   ~MapPopoutFrame() override = default;

   MapPopoutFrame(const MapPopoutFrame&)            = delete;
   MapPopoutFrame& operator=(const MapPopoutFrame&) = delete;
   MapPopoutFrame(MapPopoutFrame&&)                 = delete;
   MapPopoutFrame& operator=(MapPopoutFrame&&)      = delete;

   void DetachMapWidget();
   void SetEmbeddedMap(scwx::qt::map::MapWidget* map);

   [[nodiscard]] std::size_t map_index() const { return mapIndex_; }

signals:
   // User closed the window (X) or similar — host should re-dock the map.
   void DockMapRequested();

protected:
   void closeEvent(QCloseEvent* event) override;

private:
   std::size_t     mapIndex_;
   QVBoxLayout*    vbox_ {};
   map::MapWidget* map_ {};
};

} // namespace scwx::qt::main
