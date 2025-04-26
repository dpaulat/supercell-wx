#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QWidget>
#include <QVBoxLayout>
#include <QPropertyAnimation>

namespace scwx {
namespace qt {
namespace main {

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   explicit MainWindow(QWidget* parent = nullptr);
   ~MainWindow();

private slots:
   void ToggleRadarProductPanel();

private:
   void SetupUi();
   void SetupRadarProductPanel();

   // UI Elements
   QWidget* centralWidget_ {nullptr};
   QVBoxLayout* mainLayout_ {nullptr};

   QPushButton* productToggleButton_ {nullptr};
   QWidget* radarProductPanel_ {nullptr};
   QVBoxLayout* radarProductLayout_ {nullptr};

   QPropertyAnimation* slideAnimation_ {nullptr};
   bool isProductPanelVisible_ {true};
};

} // namespace main
} // namespace qt
} // namespace scwx
