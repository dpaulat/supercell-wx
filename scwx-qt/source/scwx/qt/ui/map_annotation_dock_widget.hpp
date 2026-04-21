#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <QWidget>

namespace scwx::qt::map
{
class MapAnnotationLayer;
}

namespace scwx::qt::ui
{

class MapAnnotationDockWidget : public QWidget
{
   Q_OBJECT

public:
   explicit MapAnnotationDockWidget(QWidget* parent = nullptr);
   ~MapAnnotationDockWidget() override;

   /** @param syncUiFromLayer if false, keep dock widgets as-is and push them to the layer (first attach). */
   void BindToLayer(const std::shared_ptr<map::MapAnnotationLayer>& layer,
                    bool syncUiFromLayer = true);

   /**
    * Layers returned here receive SetTool / SetStyle / ClearAll from the dock.
    * The layer passed to BindToLayer() is still used for signals (measure, count)
    * and for PullStyleToUi when the active map changes.
    */
   void SetBroadcastTargets(
      std::function<std::vector<std::shared_ptr<map::MapAnnotationLayer>>()> getLayers);

   /** Re-run current tool combo + style widgets onto all broadcast targets. */
   void ReapplyToolAndStyleFromUi();

   void AttachToMap(QWidget* mapWidget);
   void SetOverlayVisible(bool visible);
   [[nodiscard]] bool OverlayVisible() const;

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
