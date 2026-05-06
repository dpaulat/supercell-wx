#include <scwx/qt/map/map_pane_context_menu.hpp>
#include <scwx/qt/map/map_widget.hpp>

#include <scwx/common/products.hpp>

#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QLabel>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringLiteral>
#include <QTimer>
#include <QWidget>
#include <QWidgetAction>

#include <string>

namespace scwx::qt::map
{
namespace
{
void AlignMapPaneSubMenuToParentMenu(const QPointer<QMenu>& sub)
{
   if (sub.isNull() || !sub->isVisible())
   {
      return;
   }
   auto* parentMenu = qobject_cast<QMenu*>(sub->parentWidget());
   if (parentMenu == nullptr)
   {
      parentMenu = qobject_cast<QMenu*>(sub->parent());
   }
   if (parentMenu == nullptr || !parentMenu->isVisible())
   {
      return;
   }
   const QRect pr = parentMenu->frameGeometry();
   const QRect sr = sub->frameGeometry();

   // If Qt flipped the submenu to the left to stay on-screen, keep it there.
   if (sr.right() < pr.left() || sr.left() < pr.left())
   {
      return;
   }

   const int dx = (pr.right() + 1) - sr.left();
   if (dx > 0)
   {
      sub->move(sub->x() + dx, sub->y());
   }
}

class MapPaneSubMenuShowAlign final : public QObject
{
public:
   explicit MapPaneSubMenuShowAlign(QMenu* submenu) :
       QObject {submenu}, submenu_ {submenu}
   {
      submenu_->installEventFilter(this);
   }

protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
      if (watched != submenu_ || event->type() != QEvent::Show)
      {
         return false;
      }
      const QPointer<QMenu> sub = submenu_;
      static constexpr int  kRetryMs {32};
      QTimer::singleShot(
         0, this, [sub]() { AlignMapPaneSubMenuToParentMenu(sub); });
      QTimer::singleShot(
         kRetryMs, this, [sub]() { AlignMapPaneSubMenuToParentMenu(sub); });
      return false;
   }

private:
   QMenu* submenu_;
};

QString MapPaneContextMenuBaseStyle(QLatin1String menuPadding,
                                    bool          clearInteriorChildBackgrounds)
{
   QString sheet = QStringLiteral(
                      "QMenu#MapPaneContextMenu {"
                      "  background-color: palette(window);"
                      "  color: palette(window-text);"
                      "  border: 1px solid palette(mid);"
                      "  border-radius: 12px;"
                      "  padding: %1;"
                      "}"
                      "QMenu#MapPaneContextMenu::item {"
                      "  padding: 8px 28px 8px 14px;"
                      "  background: palette(button);"
                      "  color: palette(button-text);"
                      "  border-radius: 4px;"
                      "  margin-top: 2px;"
                      "}"
                      "QMenu#MapPaneContextMenu::item:selected {"
                      "  background-color: palette(highlight);"
                      "  color: palette(highlighted-text);"
                      "  border: 1px solid palette(highlight);"
                      "}"
                      "QMenu#MapPaneContextMenu::item:pressed {"
                      "  background-color: palette(highlight);"
                      "  color: palette(highlighted-text);"
                      "}"
                      "QMenu#MapPaneContextMenu::item:disabled {"
                      "  color: palette(mid);"
                      "  background: palette(window);"
                      "}"
                      "QMenu#MapPaneContextMenu::separator {"
                      "  height: 1px;"
                      "  background: palette(mid);"
                      "  margin: 0 2px 8px 2px;"
                      "}"
                      "QMenu#MapPaneContextMenu::indicator {"
                      "  width: 16px; height: 16px; left: 8px;"
                      "}")
                      .arg(menuPadding);
   if (clearInteriorChildBackgrounds)
   {
      sheet += QStringLiteral(
         "QMenu#MapPaneContextMenu QWidget {"
         "  background: transparent;"
         "}");
   }
   return sheet;
}

QString MapPaneContextMenuStyleSheet()
{
   return MapPaneContextMenuBaseStyle(QLatin1String {"14px 12px 12px 12px"},
                                      true);
}

QString MapPaneContextSubMenuStyleSheet()
{
   return MapPaneContextMenuBaseStyle(QLatin1String {"8px 10px 8px 10px"},
                                      false);
}

void StyleMapPaneContextSubmenu(QMenu* m)
{
   if (m == nullptr)
   {
      return;
   }
   m->setObjectName(QStringLiteral("MapPaneContextMenu"));
   m->setAttribute(Qt::WA_TranslucentBackground, true);
   m->setAttribute(Qt::WA_StyledBackground, true);
   m->setMinimumWidth(0);
   m->setStyleSheet(MapPaneContextSubMenuStyleSheet());
   static constexpr const char* kAlignInstalled = "_scwx_alignInstalled";
   if (!m->property(kAlignInstalled).toBool())
   {
      m->setProperty(kAlignInstalled, true);
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new MapPaneSubMenuShowAlign(m);
   }
}
} // namespace

void AppendMapPaneRadarContextMenu(
   QMenu&     menu,
   MapWidget* map,
   QObject*   connect_target,
   const std::function<
      void(MapWidget*, common::RadarProductGroup, const std::string&, int16_t)>&
                                              on_select,
   const std::function<QString(const char*)>& trFn)
{
   const common::RadarProductGroup currentGroup = map->GetRadarProductGroup();
   const std::string               currentName  = map->GetRadarProductName();

   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
   auto* const productActionGroup = new QActionGroup {&menu};
   productActionGroup->setExclusive(true);

   QMenu* const l2Menu = menu.addMenu(trFn("L2 &Product"));
   StyleMapPaneContextSubmenu(l2Menu);

   for (const common::Level2Product product : common::Level2ProductIterator())
   {
      const std::string& name = common::GetLevel2Name(product);
      QAction* const     a    = l2Menu->addAction(QString::fromStdString(name));
      a->setCheckable(true);
      a->setActionGroup(productActionGroup);
      a->setChecked(currentGroup == common::RadarProductGroup::Level2 &&
                    currentName == name);
      QObject::connect(
         a,
         &QAction::triggered,
         connect_target,
         [map, name, on_select]()
         { on_select(map, common::RadarProductGroup::Level2, name, 0); });
   }

   QMenu* const l3Menu = menu.addMenu(trFn("L3 &Product"));
   StyleMapPaneContextSubmenu(l3Menu);
   const common::Level3ProductCategoryMap& l3Avail =
      map->GetAvailableLevel3Categories();
   bool anyL3 = false;

   for (const common::Level3ProductCategory category :
        common::Level3ProductCategoryIterator())
   {
      const auto catIt = l3Avail.find(category);
      if (catIt == l3Avail.cend() || catIt->second.empty())
      {
         continue;
      }

      anyL3 = true;
      const std::string awips =
         common::GetLevel3CategoryDefaultProduct(category, l3Avail);
      QAction* const a = l3Menu->addAction(
         QString::fromStdString(common::GetLevel3CategoryName(category)));
      a->setCheckable(true);
      a->setActionGroup(productActionGroup);
      a->setToolTip(QString::fromStdString(
         common::GetLevel3CategoryDescription(category)));
      const bool inCategory =
         currentGroup == common::RadarProductGroup::Level3 &&
         common::GetLevel3CategoryByAwipsId(currentName) == category;
      a->setChecked(inCategory);
      QObject::connect(
         a,
         &QAction::triggered,
         connect_target,
         [map, awips, on_select]()
         { on_select(map, common::RadarProductGroup::Level3, awips, 0); });
   }

   l3Menu->setEnabled(anyL3);
}

void RunMapPaneContextMenu(const MapPaneContextMenuConfig& cfg,
                           const QPoint&                   globalPos)
{
   if (cfg.current_map == nullptr || cfg.event_receiver == nullptr ||
       cfg.maps == nullptr || cfg.view_linked == nullptr ||
       cfg.popped_out == nullptr || !cfg.append_radar_submenus)
   {
      return;
   }

   const QPointer<QObject>                receiver {cfg.event_receiver};
   const std::size_t                      mapIndex = cfg.map_index;
   MapWidget* const                       curMap   = cfg.current_map;
   const std::function<void(std::size_t)> onPop    = cfg.on_popout;
   const std::function<void(std::size_t)> onDock   = cfg.on_dock;
   const std::function<void(std::size_t, MapWidget*, bool)> onLink =
      cfg.on_link_toggled;
   const std::function<void()>                   onReset = cfg.on_reset_layout;
   const std::function<void(QMenu&, MapWidget*)> appendRadar =
      cfg.append_radar_submenus;

   QMenu menu {cfg.menu_parent};
   menu.setObjectName(QStringLiteral("MapPaneContextMenu"));
   menu.setAttribute(Qt::WA_TranslucentBackground);
   menu.setAttribute(Qt::WA_StyledBackground, true);
   menu.setMinimumWidth(0);
   menu.setStyleSheet(MapPaneContextMenuStyleSheet());

   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
   auto* const titleLabel = new QLabel(cfg.title_map, &menu);
   titleLabel->setObjectName(QStringLiteral("MapPaneContextMenuTitle"));
   titleLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
   titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
   titleLabel->setStyleSheet(
      QStringLiteral("QLabel#MapPaneContextMenuTitle {"
                     "  color: palette(window-text);"
                     "  font-weight: 600;"
                     "  font-size: 13px;"
                     "  padding: 0 0 0 0;"
                     "  margin: 0 0 0 0;"
                     "  background: transparent;"
                     "  border: none;"
                     "}"));
   // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
   auto* const titleAction = new QWidgetAction(&menu);
   titleAction->setDefaultWidget(titleLabel);
   menu.addAction(titleAction);
   menu.addSeparator();

   if (mapIndex < cfg.popped_out->size() && !cfg.popped_out->at(mapIndex) &&
       onPop)
   {
      QAction* const popoutAct = menu.addAction(cfg.text_popout);
      QObject::connect(popoutAct,
                       &QAction::triggered,
                       receiver,
                       [receiver, mapIndex, onPop]()
                       {
                          QTimer::singleShot(0,
                                             receiver,
                                             [receiver, mapIndex, onPop]()
                                             { onPop(mapIndex); });
                       });
   }
   if (mapIndex < cfg.popped_out->size() && cfg.popped_out->at(mapIndex) &&
       onDock)
   {
      QAction* const dockAct = menu.addAction(cfg.text_dock);
      QObject::connect(dockAct,
                       &QAction::triggered,
                       receiver,
                       [receiver, mapIndex, onDock]()
                       {
                          QTimer::singleShot(0,
                                             receiver,
                                             [receiver, mapIndex, onDock]()
                                             { onDock(mapIndex); });
                       });
   }
   menu.addSeparator();

   QAction* const linkViewAction = menu.addAction(cfg.text_link);
   linkViewAction->setCheckable(true);
   linkViewAction->setEnabled(cfg.maps->size() > 1u);
   if (linkViewAction->isEnabled())
   {
      linkViewAction->setToolTip(cfg.tooltip_link_enabled);
   }
   else
   {
      linkViewAction->setToolTip(cfg.tooltip_link_disabled);
   }
   {
      const QSignalBlocker blocker {linkViewAction};
      linkViewAction->setChecked(mapIndex < cfg.view_linked->size() &&
                                 cfg.view_linked->at(mapIndex));
   }
   if (linkViewAction->isEnabled() && onLink)
   {
      QObject::connect(linkViewAction,
                       &QAction::toggled,
                       receiver,
                       [mapIndex, curMap, onLink](bool linked)
                       { onLink(mapIndex, curMap, linked); });
   }

   menu.addSeparator();
   appendRadar(menu, curMap);

   QAction* const resetLayoutAction = menu.addAction(cfg.text_reset_layout);
   const bool     popped =
      mapIndex < cfg.popped_out->size() && cfg.popped_out->at(mapIndex);
   resetLayoutAction->setEnabled(!popped);
   if (popped)
   {
      resetLayoutAction->setToolTip(cfg.tooltip_reset_layout_when_popped);
   }
   QObject::connect(resetLayoutAction,
                    &QAction::triggered,
                    receiver,
                    [onReset]()
                    {
                       if (onReset)
                       {
                          onReset();
                       }
                    });

   menu.adjustSize();
   menu.exec(globalPos);
}

} // namespace scwx::qt::map
