#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/common/characters.hpp>
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

class MainWindowImpl : public QObject
{
   Q_OBJECT

public:
   explicit MainWindowImpl(MainWindow* mainWindow) :
       mainWindow_ {mainWindow},
       map_ {nullptr},
       elevationCuts_ {},
       resizeElevationButtons_ {false}
   {
   }
   ~MainWindowImpl() = default;

   void InitializeConnections();
   void SelectElevation(map::MapWidget* mapWidget, float elevation);
   void SelectRadarProduct(common::Level2Product product);
   void UpdateElevationSelection(float elevation);
   void UpdateRadarProductSettings(map::MapWidget* mapWidget);

   MainWindow*     mainWindow_;
   map::MapWidget* map_;

   std::vector<float> elevationCuts_;

   bool resizeElevationButtons_;
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
   level2Layout->setContentsMargins(0, 0, 0, 0);
   ui->level2ProductFrame->setLayout(level2Layout);

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

   QLayout* elevationLayout = new ui::FlowLayout();
   ui->elevationGroupBox->setLayout(elevationLayout);

   ui->settingsGroupBox->setVisible(false);
   ui->declutterCheckbox->setVisible(false);

   p->InitializeConnections();

   p->SelectRadarProduct(common::Level2Product::Reflectivity);
}

MainWindow::~MainWindow()
{
   delete ui;
}

bool MainWindow::event(QEvent* event)
{
   if (event->type() == QEvent::Type::Paint)
   {
      if (p->resizeElevationButtons_)
      {
         // Set each elevation cut's tool button to the same size
         int elevationCutMaxWidth = 0;
         for (QToolButton* widget :
              ui->elevationGroupBox->findChildren<QToolButton*>())
         {
            elevationCutMaxWidth =
               std::max(elevationCutMaxWidth, widget->width());
         }
         for (QToolButton* widget :
              ui->elevationGroupBox->findChildren<QToolButton*>())
         {
            widget->setMinimumWidth(elevationCutMaxWidth);
         }

         p->resizeElevationButtons_ = false;
      }
   }

   return QMainWindow::event(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   // Set each level 2 product's tool button to the same size
   int level2MaxWidth = 0;
   for (QToolButton* widget :
        ui->level2ProductFrame->findChildren<QToolButton*>())
   {
      level2MaxWidth = std::max(level2MaxWidth, widget->width());
   }
   for (QToolButton* widget :
        ui->level2ProductFrame->findChildren<QToolButton*>())
   {
      widget->setMinimumWidth(level2MaxWidth);
   }

   // Set each elevation cut's tool button to the same size
   int elevationCutMaxWidth = 0;
   for (QToolButton* widget :
        ui->elevationGroupBox->findChildren<QToolButton*>())
   {
      elevationCutMaxWidth = std::max(elevationCutMaxWidth, widget->width());
   }
   for (QToolButton* widget :
        ui->elevationGroupBox->findChildren<QToolButton*>())
   {
      widget->setMinimumWidth(elevationCutMaxWidth);
   }

   resizeDocks({ui->radarToolboxDock}, {150}, Qt::Horizontal);
}

void MainWindowImpl::InitializeConnections()
{
   connect(
      map_,
      &map::MapWidget::RadarSweepUpdated,
      this,
      [this]() { UpdateRadarProductSettings(map_); },
      Qt::QueuedConnection);
}

void MainWindowImpl::SelectElevation(map::MapWidget* mapWidget, float elevation)
{
   mapWidget->SelectElevation(elevation);
   UpdateElevationSelection(elevation);
}

void MainWindowImpl::SelectRadarProduct(common::Level2Product product)
{
   const std::string& productName = common::GetLevel2Name(product);

   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Selecting Level 2 radar product: " << productName;

   for (QToolButton* toolButton :
        mainWindow_->ui->level2ProductFrame->findChildren<QToolButton*>())
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

void MainWindowImpl::UpdateElevationSelection(float elevation)
{
   QString buttonText {QString::number(elevation, 'f', 1) +
                       common::Characters::DEGREE};

   for (QToolButton* toolButton :
        mainWindow_->ui->elevationGroupBox->findChildren<QToolButton*>())
   {
      if (toolButton->text() == buttonText)
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
}

void MainWindowImpl::UpdateRadarProductSettings(map::MapWidget* mapWidget)
{
   float              currentElevation = mapWidget->GetElevation();
   std::vector<float> elevationCuts    = mapWidget->GetElevationCuts();

   if (elevationCuts_ != elevationCuts)
   {
      for (QToolButton* toolButton :
           mainWindow_->ui->elevationGroupBox->findChildren<QToolButton*>())
      {
         delete toolButton;
      }

      QLayout* layout = mainWindow_->ui->elevationGroupBox->layout();

      // Create elevation cut tool buttons
      for (float elevationCut : elevationCuts)
      {
         QToolButton* toolButton = new QToolButton();
         toolButton->setText(QString::number(elevationCut, 'f', 1) +
                             common::Characters::DEGREE);
         layout->addWidget(toolButton);

         connect(toolButton, &QToolButton::clicked, this, [=]() {
            SelectElevation(mapWidget, elevationCut);
         });
      }

      elevationCuts_          = elevationCuts;
      resizeElevationButtons_ = true;
   }

   UpdateElevationSelection(currentElevation);
}

} // namespace main
} // namespace qt
} // namespace scwx

#include "main_window.moc"
