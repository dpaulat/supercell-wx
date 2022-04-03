#define NOMINMAX

#include "main_window.hpp"
#include "./ui_main_window.h"

#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/products.hpp>
#include <scwx/common/vcp.hpp>

#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
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
       settings_ {},
       activeMap_ {nullptr},
       maps_ {},
       elevationCuts_ {},
       elevationButtonsChanged_ {false},
       resizeElevationButtons_ {false}
   {
      settings_.setCacheDatabasePath("/tmp/mbgl-cache.db");
      settings_.setCacheDatabaseMaximumSize(20 * 1024 * 1024);
   }
   ~MainWindowImpl() = default;

   void ConfigureMapLayout();
   void HandleFocusChange(QWidget* focused);
   void NormalizeElevationButtons();
   void NormalizeLevel2ProductButtons();
   void SelectElevation(map::MapWidget* mapWidget, float elevation);
   void SelectRadarProduct(map::MapWidget*       mapWidget,
                           common::Level2Product product);
   void SetActiveMap(map::MapWidget* mapWidget);
   void UpdateElevationSelection(float elevation);
   void UpdateLevel2ProductSelection(common::Level2Product product);
   void UpdateRadarProductSelection(common::RadarProductGroup group,
                                    const std::string&        product);
   void UpdateRadarProductSettings();
   void UpdateVcp();

   MainWindow*       mainWindow_;
   QMapboxGLSettings settings_;
   map::MapWidget*   activeMap_;

   std::vector<map::MapWidget*> maps_;
   std::vector<float>           elevationCuts_;

   bool elevationButtonsChanged_;
   bool resizeElevationButtons_;

public slots:
   void UpdateMapParameters(double latitude,
                            double longitude,
                            double zoom,
                            double bearing,
                            double pitch);
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    p(std::make_unique<MainWindowImpl>(this)),
    ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   ui->vcpLabel->setVisible(false);
   ui->vcpValueLabel->setVisible(false);
   ui->vcpDescriptionLabel->setVisible(false);

   p->ConfigureMapLayout();

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

      connect(toolButton,
              &QToolButton::clicked,
              this,
              [=]() { p->SelectRadarProduct(p->activeMap_, product); });
   }

   QLayout* elevationLayout = new ui::FlowLayout();
   ui->elevationGroupBox->setLayout(elevationLayout);

   ui->settingsGroupBox->setVisible(false);
   ui->declutterCheckbox->setVisible(false);

   p->SelectRadarProduct(p->activeMap_, common::Level2Product::Reflectivity);
   if (p->maps_.at(1) != nullptr)
   {
      p->SelectRadarProduct(p->maps_.at(1), common::Level2Product::Velocity);
   }

   connect(qApp,
           &QApplication::focusChanged,
           this,
           [=](QWidget* old, QWidget* now) { p->HandleFocusChange(now); });
}

MainWindow::~MainWindow()
{
   delete ui;
}

bool MainWindow::event(QEvent* event)
{
   if (event->type() == QEvent::Type::Paint)
   {
      if (p->elevationButtonsChanged_)
      {
         p->elevationButtonsChanged_ = false;
      }
      else if (p->resizeElevationButtons_)
      {
         p->NormalizeElevationButtons();
      }
   }

   return QMainWindow::event(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
   QMainWindow::showEvent(event);

   p->NormalizeLevel2ProductButtons();
   p->NormalizeElevationButtons();

   resizeDocks({ui->radarToolboxDock}, {150}, Qt::Horizontal);
}

void MainWindowImpl::NormalizeLevel2ProductButtons()
{
   // Set each level 2 product's tool button to the same size
   int level2MaxWidth = 0;
   for (QToolButton* widget :
        mainWindow_->ui->level2ProductFrame->findChildren<QToolButton*>())
   {
      if (widget->isVisible())
      {
         level2MaxWidth = std::max(level2MaxWidth, widget->width());
      }
   }

   if (level2MaxWidth > 0)
   {
      for (QToolButton* widget :
           mainWindow_->ui->level2ProductFrame->findChildren<QToolButton*>())
      {
         widget->setMinimumWidth(level2MaxWidth);
      }
   }
}

void MainWindowImpl::NormalizeElevationButtons()
{
   // Set each elevation cut's tool button to the same size
   int elevationCutMaxWidth = 0;
   for (QToolButton* widget :
        mainWindow_->ui->elevationGroupBox->findChildren<QToolButton*>())
   {
      if (widget->isVisible())
      {
         elevationCutMaxWidth = std::max(elevationCutMaxWidth, widget->width());
      }
   }

   if (elevationCutMaxWidth > 0)
   {
      for (QToolButton* widget :
           mainWindow_->ui->elevationGroupBox->findChildren<QToolButton*>())
      {
         widget->setMinimumWidth(elevationCutMaxWidth);
      }

      resizeElevationButtons_ = false;
   }
}

void MainWindow::on_actionOpen_triggered()
{
   static const std::string nexradFilter = "NEXRAD Products (*)";

   QFileDialog* dialog = new QFileDialog(this);

   dialog->setFileMode(QFileDialog::ExistingFile);
   dialog->setNameFilter(tr(nexradFilter.c_str()));
   dialog->setAttribute(Qt::WA_DeleteOnClose);

   map::MapWidget* currentMap = p->activeMap_;

   // Make sure the parent window properly repaints on close
   connect(
      dialog,
      &QFileDialog::finished,
      this,
      [=]() { update(); },
      Qt::QueuedConnection);

   connect(
      dialog,
      &QFileDialog::fileSelected,
      this,
      [=](const QString& file)
      {
         BOOST_LOG_TRIVIAL(info) << "Selected: " << file.toStdString();

         std::shared_ptr<request::NexradFileRequest> request =
            std::make_shared<request::NexradFileRequest>();

         connect( //
            request.get(),
            &request::NexradFileRequest::RequestComplete,
            this,
            [=](std::shared_ptr<request::NexradFileRequest> request)
            {
               std::shared_ptr<types::RadarProductRecord> record =
                  request->radar_product_record();

               if (record != nullptr)
               {
                  currentMap->SelectRadarProduct(record->radar_id(),
                                                 record->radar_product_group(),
                                                 record->radar_product(),
                                                 record->time());
               }
               else
               {
                  QMessageBox* messageBox = new QMessageBox(this);
                  messageBox->setIcon(QMessageBox::Warning);
                  messageBox->setText(
                     QString("%1\n%2").arg(tr("Unrecognized NEXRAD Product:"),
                                           QDir::toNativeSeparators(file)));
                  messageBox->setAttribute(Qt::WA_DeleteOnClose);
                  messageBox->open();
               }
            });

         manager::RadarProductManager::LoadFile(file.toStdString(), request);
      });

   dialog->open();
}

void MainWindow::on_actionExit_triggered()
{
   close();
}

void MainWindowImpl::ConfigureMapLayout()
{
   auto generalSettings = manager::SettingsManager::general_settings();

   const int64_t gridWidth  = generalSettings->grid_width();
   const int64_t gridHeight = generalSettings->grid_height();
   const int64_t mapCount   = gridWidth * gridHeight;

   size_t mapIndex = 0;

   QSplitter* vs = new QSplitter(Qt::Vertical);
   vs->setHandleWidth(1);

   maps_.resize(mapCount);

   auto MoveSplitter = [=](int pos, int index)
   {
      QSplitter* s = dynamic_cast<QSplitter*>(sender());

      if (s != nullptr)
      {
         auto sizes = s->sizes();
         for (QSplitter* hs : vs->findChildren<QSplitter*>())
         {
            hs->setSizes(sizes);
         }
      }
   };

   for (int64_t y = 0; y < gridHeight; y++)
   {
      QSplitter* hs = new QSplitter(vs);
      hs->setHandleWidth(1);

      for (int64_t x = 0; x < gridWidth; x++, mapIndex++)
      {
         if (maps_.at(mapIndex) == nullptr)
         {
            maps_[mapIndex] = new map::MapWidget(settings_);

            connect(maps_[mapIndex],
                    &map::MapWidget::MapParametersChanged,
                    this,
                    &MainWindowImpl::UpdateMapParameters);

            connect(
               maps_[mapIndex],
               &map::MapWidget::RadarSweepUpdated,
               this,
               [=]()
               {
                  if (maps_[mapIndex] == activeMap_)
                  {
                     UpdateVcp();
                     UpdateRadarProductSettings();
                  }
               },
               Qt::QueuedConnection);
         }

         hs->addWidget(maps_[mapIndex]);
      }

      connect(hs, &QSplitter::splitterMoved, this, MoveSplitter);
   }

   mainWindow_->ui->centralwidget->layout()->addWidget(vs);

   SetActiveMap(maps_.at(0));
}

void MainWindowImpl::HandleFocusChange(QWidget* focused)
{
   map::MapWidget* mapWidget = dynamic_cast<map::MapWidget*>(focused);

   if (mapWidget != nullptr)
   {
      SetActiveMap(mapWidget);
      UpdateRadarProductSelection(mapWidget->GetRadarProductGroup(),
                                  mapWidget->GetRadarProductName());
      UpdateRadarProductSettings();
      UpdateVcp();
   }
}

void MainWindowImpl::SelectElevation(map::MapWidget* mapWidget, float elevation)
{
   mapWidget->SelectElevation(elevation);

   if (mapWidget == activeMap_)
   {
      UpdateElevationSelection(elevation);
   }
}

void MainWindowImpl::SelectRadarProduct(map::MapWidget*       mapWidget,
                                        common::Level2Product product)
{
   const std::string& productName = common::GetLevel2Name(product);

   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Selecting Level 2 radar product: " << productName;

   if (mapWidget == activeMap_)
   {
      UpdateLevel2ProductSelection(product);
      UpdateRadarProductSettings();
   }

   mapWidget->SelectRadarProduct(product);
}

void MainWindowImpl::SetActiveMap(map::MapWidget* mapWidget)
{
   if (mapWidget == activeMap_)
   {
      return;
   }

   activeMap_ = mapWidget;

   for (map::MapWidget* widget : maps_)
   {
      widget->SetActive(mapWidget == widget);
   }
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

void MainWindowImpl::UpdateMapParameters(
   double latitude, double longitude, double zoom, double bearing, double pitch)
{
   for (map::MapWidget* map : maps_)
   {
      map->SetMapParameters(latitude, longitude, zoom, bearing, pitch);
   }
}

void MainWindowImpl::UpdateLevel2ProductSelection(common::Level2Product product)
{
   const std::string& productName = common::GetLevel2Name(product);

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
}

void MainWindowImpl::UpdateRadarProductSelection(
   common::RadarProductGroup group, const std::string& product)
{
   switch (group)
   {
   case common::RadarProductGroup::Level2:
      UpdateLevel2ProductSelection(common::GetLevel2Product(product));
      break;

   default: UpdateLevel2ProductSelection(common::Level2Product::Unknown); break;
   }
}

void MainWindowImpl::UpdateRadarProductSettings()
{
   float              currentElevation = activeMap_->GetElevation();
   std::vector<float> elevationCuts    = activeMap_->GetElevationCuts();

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

         connect(toolButton,
                 &QToolButton::clicked,
                 this,
                 [=]() { SelectElevation(activeMap_, elevationCut); });
      }

      elevationCuts_           = elevationCuts;
      elevationButtonsChanged_ = true;
      resizeElevationButtons_  = true;
   }

   UpdateElevationSelection(currentElevation);
}

void MainWindowImpl::UpdateVcp()
{
   uint16_t vcp = activeMap_->GetVcp();

   if (vcp != 0)
   {
      mainWindow_->ui->vcpLabel->setVisible(true);
      mainWindow_->ui->vcpValueLabel->setVisible(true);
      mainWindow_->ui->vcpDescriptionLabel->setVisible(true);

      mainWindow_->ui->vcpValueLabel->setText(QString::number(vcp));
      mainWindow_->ui->vcpDescriptionLabel->setText(
         tr(common::GetVcpDescription(vcp).c_str()));
   }
   else
   {
      mainWindow_->ui->vcpLabel->setVisible(false);
      mainWindow_->ui->vcpValueLabel->setVisible(false);
      mainWindow_->ui->vcpDescriptionLabel->setVisible(false);
   }
}

} // namespace main
} // namespace qt
} // namespace scwx

#include "main_window.moc"
