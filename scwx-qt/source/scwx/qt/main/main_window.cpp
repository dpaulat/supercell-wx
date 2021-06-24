#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/map/map_widget.hpp>

namespace scwx
{
namespace qt
{

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   QMapboxGLSettings settings;
   settings.setCacheDatabasePath("/tmp/mbgl-cache.db");
   settings.setCacheDatabaseMaximumSize(20 * 1024 * 1024);

   ui->centralwidget->layout()->addWidget(new MapWidget(settings));
}

MainWindow::~MainWindow()
{
   delete ui;
}

} // namespace qt
} // namespace scwx
