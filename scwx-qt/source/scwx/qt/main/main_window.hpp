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

   void showEvent(QShowEvent* event) override;

signals:
   void ActiveMapMoved(double latitude, double longitude);

private slots:
   void on_actionOpen_triggered();
   void on_actionExit_triggered();
   void on_radarSiteSelectButton_clicked();
   void on_resourceTreeCollapseAllButton_clicked();
   void on_resourceTreeExpandAllButton_clicked();

private:
   std::unique_ptr<MainWindowImpl> p;
   Ui::MainWindow*                 ui;

   friend class MainWindowImpl;
};

} // namespace main
} // namespace qt
} // namespace scwx
