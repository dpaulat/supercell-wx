#pragma once

#include <scwx/common/products.hpp>

#include <QMenu>
#include <QPoint>
#include <QString>

#include <cstddef>
#include <functional>
#include <vector>

class QObject;
class QWidget;

namespace scwx::qt::map
{
class MapWidget;

struct MapPaneContextMenuConfig
{
   QObject* event_receiver = nullptr;
   /// Parent for QMenu: typically \c map->window().
   QWidget* menu_parent = nullptr;
   // Pre-translated strings (caller has MainWindow::tr / ui access).
   QString title_map;
   QString text_popout;
   QString text_dock;
   QString text_link;
   QString tooltip_link_enabled;
   QString tooltip_link_disabled;
   QString text_reset_layout;
   QString tooltip_reset_layout_when_popped;

   std::size_t                    map_index   = 0;
   const std::vector<MapWidget*>* maps        = nullptr;
   const std::vector<bool>*       view_linked = nullptr;
   const std::vector<bool>*       popped_out  = nullptr;
   MapWidget*                     current_map = nullptr;

   std::function<void(std::size_t)> on_popout;
   std::function<void(std::size_t)> on_dock;
   /// Invoked with (map_index, map, linked) when "Link view" is toggled.
   std::function<void(std::size_t, MapWidget*, bool linked)> on_link_toggled;
   std::function<void()>                                     on_reset_layout;
   /// Appends L2/L3 product submenus (and connects actions).
   std::function<void(QMenu& menu, MapWidget* map)> append_radar_submenus;
};

void RunMapPaneContextMenu(const MapPaneContextMenuConfig& cfg,
                           const QPoint&                   globalPos);

/**
 * @brief Appends L2 and L3 product submenus to an existing context menu.
 *
 * @param [in] menu Parent menu. QActionGroup and actions are parented to it.
 * @param [in] map Map whose products are shown.
 * @param [in] connect_target Receiver for \c QAction::triggered connections.
 * @param [in] on_select Invoked to apply a product selection.
 * @param [in] trFn Translation function; typically <code>mainWindow->tr</code>
 * for \a connect_target's top-level window.
 */
void AppendMapPaneRadarContextMenu(
   QMenu&     menu,
   MapWidget* map,
   QObject*   connect_target,
   const std::function<
      void(MapWidget*, common::RadarProductGroup, const std::string&, int16_t)>&
                                              on_select,
   const std::function<QString(const char*)>& trFn);

} // namespace scwx::qt::map
