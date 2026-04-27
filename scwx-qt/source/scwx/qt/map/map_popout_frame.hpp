#pragma once

#include <QWidget>

#include <cstddef>
#include <memory>

class QCloseEvent;

namespace scwx::qt::map
{

class MapWidget;

// Borderless top-level window holding a single map pane (no main menu, no
// docks).
class MapPopoutFrame : public QWidget
{
   Q_OBJECT

public:
   explicit MapPopoutFrame(std::size_t mapIndex, QWidget* parent = nullptr);
   ~MapPopoutFrame() override;

   MapPopoutFrame(const MapPopoutFrame&)            = delete;
   MapPopoutFrame& operator=(const MapPopoutFrame&) = delete;
   MapPopoutFrame(MapPopoutFrame&&)                 = delete;
   MapPopoutFrame& operator=(MapPopoutFrame&&)      = delete;

   void        DetachMapWidget();
   void        SetEmbeddedMap(MapWidget* map);
   std::size_t map_index() const;

signals:
   // User closed the window (X) or similar — host should re-dock the map.
   void DockMapRequested();

protected:
   void closeEvent(QCloseEvent* event) override;

private:
   class Impl;
   std::unique_ptr<Impl> p_;
};

} // namespace scwx::qt::map
