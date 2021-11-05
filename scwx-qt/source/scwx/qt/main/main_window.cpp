#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/common/products.hpp>

#include <QToolButton>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace main
{

static const std::string logPrefix_ = "[scwx::qt::main::main_window] ";

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   QMapboxGLSettings settings;
   settings.setCacheDatabasePath("/tmp/mbgl-cache.db");
   settings.setCacheDatabaseMaximumSize(20 * 1024 * 1024);

   ui->centralwidget->layout()->addWidget(new map::MapWidget(settings));

   // Add Level2 Products
   QLayout* level2Layout = new ui::FlowLayout();
   ui->level2Products->setLayout(level2Layout);

   for (common::Level2Product product : common::Level2ProductIterator())
   {
      QToolButton* toolButton = new QToolButton();
      toolButton->setText(
         QString::fromStdString(common::GetLevel2Name(product)));
      toolButton->setStatusTip(
         tr(common::GetLevel2Description(product).c_str()));
      level2Layout->addWidget(toolButton);
   }
}

MainWindow::~MainWindow()
{
   delete ui;
}

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   // Cycle through each item in the toolbox to render
   QToolBox* toolbox      = ui->radarToolbox;
   int       currentIndex = toolbox->currentIndex();
   for (int i = 0; i < toolbox->count(); i++)
   {
      toolbox->setCurrentIndex(i);
   }
   toolbox->setCurrentIndex(currentIndex);

   // Set each level 2 product's tool button to the same size
   int level2MaxWidth = 0;
   for (QWidget* widget : ui->level2Products->findChildren<QWidget*>())
   {
      level2MaxWidth = std::max(level2MaxWidth, widget->width());
   }
   for (QWidget* widget : ui->level2Products->findChildren<QWidget*>())
   {
      widget->setMinimumWidth(level2MaxWidth);
   }

   resizeDocks({ui->radarToolboxDock}, {150}, Qt::Horizontal);
}

} // namespace main
} // namespace qt
} // namespace scwx
