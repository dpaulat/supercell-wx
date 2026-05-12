#pragma once

#include <scwx/sounding/sounding_data.hpp>

#include <memory>

#include <QDockWidget>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSplitter;

namespace scwx::qt::view
{
class SkewtWidget;
class HodographWidget;
} // namespace scwx::qt::view

namespace scwx::qt::ui
{

class SoundingPanelImpl;

class SoundingPanel : public QDockWidget
{
   Q_OBJECT

public:
   explicit SoundingPanel(QWidget* parent = nullptr);
   ~SoundingPanel();

   SoundingPanel(const SoundingPanel&)            = delete;
   SoundingPanel& operator=(const SoundingPanel&) = delete;

   void SetLocation(double lat, double lon);
   void RequestSounding();

public slots:
   void
   OnSoundingReady(const std::shared_ptr<sounding::SoundingData>& sounding);
   void OnLoadError(const QString& message);
   void OnFetchClicked();

signals:
   /**
    * Emitted when the user clicks "Select Forecast Point" to pick a point
    * from the map. The main window should connect this to the map's
    * point-click handling logic.
    */
   void PointSelectionStarted();

private:
   std::unique_ptr<SoundingPanelImpl> p;
};

} // namespace scwx::qt::ui
