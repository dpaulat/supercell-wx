#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <QWidget>

class QEvent;
class QCloseEvent;

namespace scwx::qt::map
{
class MapAnnotationLayer;
}

namespace scwx::qt::ui
{

class MapAnnotationDockWidget : public QWidget
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(MapAnnotationDockWidget)

public:
   explicit MapAnnotationDockWidget(QWidget* parent = nullptr);
   ~MapAnnotationDockWidget() override;

   /** @param syncUiFromLayer if false, keep dock widgets as-is and push them to
    * the layer (first attach). */
   void BindToLayer(const std::shared_ptr<map::MapAnnotationLayer>& layer,
                    bool syncUiFromLayer = true);

   /**
    * Layers returned here receive SetTool / SetStyle / ClearAll from the dock.
    * The layer passed to BindToLayer() is still used for signals (measure,
    * count) and for PullStyleToUi when the active map changes.
    */
   void SetBroadcastTargets(
      std::function<std::vector<std::shared_ptr<map::MapAnnotationLayer>>()>
         getLayers);

   /**
    * After pop-out, if the attached map pane was removed, Dock uses this to
    * pick a map widget (e.g. active pane, else first pane). Empty = no rehome.
    */
   void SetFloatingDockHostResolver(std::function<QWidget*()> resolver);

   /**
    * Apply persisted pop-out after AttachToMap (LoadState runs in ctor before
    * host exists; old sessions also used global float coords vs
    * parent-relative).
    */
   void ApplyDeferredFloatingState();

   /** Re-run current tool combo + style widgets onto all broadcast targets. */
   void ReapplyToolAndStyleFromUi();

   void AttachToMap(QWidget* mapWidget);
   /** If the dock overlay / event filter is tied to |mapWidget|, detach before
    * that widget is destroyed (e.g. grid shrink removes a pane). */
   void DetachIfHostedBy(QWidget* mapWidget, bool preserveFloating = false);
   void SetOverlayVisible(bool visible);
   [[nodiscard]] bool OverlayVisible() const;

   /** Minimized toolbar (- button); no separate on-map collapsed control. */
   void               SetPanelExpanded(bool expanded);
   [[nodiscard]] bool PanelExpanded() const;

protected:
   void changeEvent(QEvent* event) override;
   void closeEvent(QCloseEvent* event) override;

private slots:
   void OnToolSelected(int toolValue);
   void OnBrushPresetChanged(int index);
   void OnFillToggled(bool on);
   void OnChooseColor();
   void OnClearAll();
   void OnToggleExpanded();

private:
   bool eventFilter(QObject* watched, QEvent* event) override;

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::ui
