#include <scwx/qt/ui/sounding_panel.hpp>
#include <scwx/qt/manager/gfs_manager.hpp>
#include <scwx/qt/view/skewt_widget.hpp>
#include <scwx/qt/view/hodograph_widget.hpp>
#include <scwx/util/logger.hpp>

#include <string>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace scwx::qt::ui
{

static const std::string logPrefix_ = "scwx::qt::ui::sounding_panel";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class SoundingPanelImpl
{
public:
   explicit SoundingPanelImpl(SoundingPanel* self) :
       self_(self),
       skewtWidget_(nullptr),
       hodographWidget_(nullptr),
       latSpinBox_(nullptr),
       lonSpinBox_(nullptr),
       cycleCombo_(nullptr),
       fhrCombo_(nullptr),
       selectPointButton_(nullptr),
       fetchButton_(nullptr)
   {
   }
   ~SoundingPanelImpl() = default;

   SoundingPanelImpl(const SoundingPanelImpl&)            = delete;
   SoundingPanelImpl& operator=(const SoundingPanelImpl&) = delete;
   SoundingPanelImpl(SoundingPanelImpl&&)                 = delete;
   SoundingPanelImpl& operator=(SoundingPanelImpl&&)      = delete;

   void SetupUi(QWidget* widget)
   {
      auto* mainLayout = new QVBoxLayout(widget);
      mainLayout->setContentsMargins(4, 4, 4, 4);

      // Controls group
      auto* controlsGroup  = new QGroupBox("GFS Sounding Controls");
      auto* controlsLayout = new QVBoxLayout(controlsGroup);

      // Location row
      auto* locLayout = new QHBoxLayout();
      locLayout->addWidget(new QLabel("Lat:"));
      latSpinBox_ = new QDoubleSpinBox();
      latSpinBox_->setRange(-90.0, 90.0);
      latSpinBox_->setDecimals(4);
      latSpinBox_->setValue(35.0);
      latSpinBox_->setSingleStep(0.1);
      locLayout->addWidget(latSpinBox_);

      locLayout->addWidget(new QLabel("Lon:"));
      lonSpinBox_ = new QDoubleSpinBox();
      lonSpinBox_->setRange(-180.0, 180.0);
      lonSpinBox_->setDecimals(4);
      lonSpinBox_->setValue(-97.0);
      lonSpinBox_->setSingleStep(0.1);
      locLayout->addWidget(lonSpinBox_);

      controlsLayout->addLayout(locLayout);

      // Model parameters row
      auto* paramLayout = new QHBoxLayout();
      paramLayout->addWidget(new QLabel("Cycle:"));
      cycleCombo_ = new QComboBox();
      cycleCombo_->addItem("00Z", 0);
      cycleCombo_->addItem("06Z", 6);
      cycleCombo_->addItem("12Z", 12);
      cycleCombo_->addItem("18Z", 18);
      paramLayout->addWidget(cycleCombo_);

      paramLayout->addWidget(new QLabel("Fhr:"));
      fhrCombo_ = new QComboBox();
      for (int f = 0; f <= 384; f += 3)
      {
         fhrCombo_->addItem(QString("F%1").arg(f, 3, 10, QChar('0')), f);
      }
      fhrCombo_->setCurrentIndex(0); // F000 (analysis)
      paramLayout->addWidget(fhrCombo_);

      controlsLayout->addLayout(paramLayout);

      // Select point and fetch buttons
      auto* buttonLayout = new QVBoxLayout();
      selectPointButton_ = new QPushButton("Select Forecast Point");
      selectPointButton_->setToolTip(
         "Click this button, then click a location on the map");
      buttonLayout->addWidget(selectPointButton_);

      fetchButton_ = new QPushButton("Fetch Sounding");
      buttonLayout->addWidget(fetchButton_);
      controlsLayout->addLayout(buttonLayout);

      mainLayout->addWidget(controlsGroup);

      // Plots
      auto* splitter = new QSplitter(Qt::Vertical);

      skewtWidget_     = new view::SkewtWidget();
      hodographWidget_ = new view::HodographWidget();

      splitter->addWidget(skewtWidget_);
      splitter->addWidget(hodographWidget_);
      splitter->setStretchFactor(0, 3);
      splitter->setStretchFactor(1, 2);

      mainLayout->addWidget(splitter, 1);

      // Connect signals
      QObject::connect(selectPointButton_,
                       &QPushButton::clicked,
                       self_,
                       [this]() { Q_EMIT self_->PointSelectionStarted(); });
      QObject::connect(fetchButton_,
                       &QPushButton::clicked,
                       self_,
                       &SoundingPanel::OnFetchClicked);

      QObject::connect(&manager::GfsManager::Instance(),
                       &manager::GfsManager::SoundingReady,
                       self_,
                       &SoundingPanel::OnSoundingReady);

      QObject::connect(&manager::GfsManager::Instance(),
                       &manager::GfsManager::LoadError,
                       self_,
                       &SoundingPanel::OnLoadError);
   }

   SoundingPanel*         self_;
   view::SkewtWidget*     skewtWidget_;
   view::HodographWidget* hodographWidget_;
   QDoubleSpinBox*        latSpinBox_;
   QDoubleSpinBox*        lonSpinBox_;
   QComboBox*             cycleCombo_;
   QComboBox*             fhrCombo_;
   QPushButton*           selectPointButton_;
   QPushButton*           fetchButton_;
};

SoundingPanel::SoundingPanel(QWidget* parent) :
    QDockWidget("GFS Sounding", parent),
    p(std::make_unique<SoundingPanelImpl>(this))
{
   setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
   setFeatures(QDockWidget::DockWidgetMovable |
               QDockWidget::DockWidgetFloatable |
               QDockWidget::DockWidgetClosable);
   setMinimumWidth(400);

   auto* widget = new QWidget();
   setWidget(widget);
   p->SetupUi(widget);
}
SoundingPanel::~SoundingPanel() = default;

void SoundingPanel::SetLocation(double lat, double lon)
{
   p->latSpinBox_->setValue(lat);
   p->lonSpinBox_->setValue(lon);
}

void SoundingPanel::RequestSounding()
{
   OnFetchClicked();
}

void SoundingPanel::OnSoundingReady(
   std::shared_ptr<sounding::SoundingData> sounding)
{
   p->skewtWidget_->SetSounding(sounding);
   p->hodographWidget_->SetSounding(sounding);
   p->fetchButton_->setEnabled(true);
   p->fetchButton_->setText("Fetch Sounding");
}

void SoundingPanel::OnLoadError(const QString& message)
{
   QMessageBox::warning(this, "GFS Sounding Error", message);
   p->fetchButton_->setEnabled(true);
   p->fetchButton_->setText("Fetch Sounding");
}

void SoundingPanel::OnFetchClicked()
{
   double lat   = p->latSpinBox_->value();
   double lon   = p->lonSpinBox_->value();
   int    cycle = p->cycleCombo_->currentData().toInt();
   int    fhr   = p->fhrCombo_->currentData().toInt();

   p->fetchButton_->setEnabled(false);
   p->fetchButton_->setText("Fetching...");

   manager::GfsManager::Instance().RequestSounding(lat, lon, cycle, fhr);
}

} // namespace scwx::qt::ui
