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

class MainWindowImpl
{
public:
   explicit MainWindowImpl(MainWindow* mainWindow) : mainWindow_(mainWindow) {}
   ~MainWindowImpl() = default;

   void SelectRadarProduct(common::Level2Product product);

   MainWindow*     mainWindow_;
   map::MapWidget* map_;
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    p(std::make_unique<MainWindowImpl>(this)),
    ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   QMapboxGLSettings settings;
   settings.setCacheDatabasePath("/tmp/mbgl-cache.db");
   settings.setCacheDatabaseMaximumSize(20 * 1024 * 1024);

   p->map_ = new map::MapWidget(settings);

   ui->centralwidget->layout()->addWidget(p->map_);

   // Add Level 2 Products
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

      connect(toolButton, &QToolButton::clicked, this, [=]() {
         p->SelectRadarProduct(product);
      });
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

void MainWindowImpl::SelectRadarProduct(common::Level2Product product)
{
   const std::string& productName = common::GetLevel2Name(product);

   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Selecting Level 2 radar product: " << productName;

   for (QToolButton* toolButton :
        mainWindow_->ui->level2Products->findChildren<QToolButton*>())
   {
      if (toolButton->text().toStdString() == productName)
      {
         toolButton->setCheckable(true);
         toolButton->setChecked(true);
      }
      else
      {
         toolButton->setChecked(false);
         toolButton->setCheckable(false);
      }
   }

   map_->SelectRadarProduct(product);
}

} // namespace main
} // namespace qt
} // namespace scwx
