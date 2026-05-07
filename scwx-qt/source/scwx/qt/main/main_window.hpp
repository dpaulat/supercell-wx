#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

namespace scwx
{
namespace qt
{
namespace main
{

class MainWindowImpl;

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   MainWindow(QWidget* parent = nullptr);
   ~MainWindow();

   void keyPressEvent(QKeyEvent* ev) override final;
   void keyReleaseEvent(QKeyEvent* ev) override final;
   void showEvent(QShowEvent* event) override;
   void closeEvent(QCloseEvent* event) override;
   void resizeEvent(QResizeEvent* event) override;
   bool eventFilter(QObject* obj, QEvent* event) override;

signals:
   void ActiveMapMoved(double latitude, double longitude);

private slots:
   void on_actionOpenNexrad_triggered();
   void on_actionOpenTextEvent_triggered();
   void on_actionScreenCaptureCopy_triggered();
   void on_actionScreenCaptureSaveImage_triggered();
   void on_actionImport_triggered();
   void on_actionExport_triggered();
   void on_actionSettings_triggered();
   void on_actionExit_triggered();
   void on_actionGpsInfo_triggered();
   void on_actionColorTable_triggered(bool checked);
   void on_actionRadarRange_triggered(bool checked);
   void on_actionRadarSites_triggered(bool checked);
   void on_actionPlacefileManager_triggered();
   void on_actionMarkerManager_triggered();
   void on_actionLayerManager_triggered();
   void on_actionImGuiDebug_triggered();
   void on_actionDumpLayerList_triggered();
   void on_actionDumpRadarProductRecords_triggered();
   void on_actionFullScreen_triggered(bool checked);
   void on_actionRadarWireframe_triggered(bool checked);
   void on_actionUserManual_triggered();
   void on_actionDiscord_triggered();
   void on_actionGitHubRepository_triggered();
   void on_actionCheckForUpdates_triggered();
   void on_actionAboutSupercellWx_triggered();
   void on_actionRecreateMapLayout_triggered();
   void on_actionPanesLinkColumnWidth_toggled(bool checked);
   void on_actionPanesLinkColumnHeight_toggled(bool checked);
   void on_actionPanesMatchMapStyle_toggled(bool checked);
   void on_actionPanes1x1_triggered();
   void on_actionPanes1x2_triggered();
   void on_actionPanes2x1_triggered();
   void on_actionPanes2x2_triggered();
   void on_actionPanes3x3_triggered();
   void on_actionPanesCustom_triggered();
   void on_radarSiteHomeButton_clicked();
   void on_radarSiteSelectButton_clicked();

private:
   std::unique_ptr<MainWindowImpl> p;
   Ui::MainWindow*                 ui;

   friend class MainWindowImpl;
};

} // namespace main
} // namespace qt
} // namespace scwx
