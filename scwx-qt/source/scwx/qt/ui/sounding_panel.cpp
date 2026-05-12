#include <scwx/qt/ui/sounding_panel.hpp>
#include <scwx/qt/manager/gfs_manager.hpp>
#include <scwx/qt/view/skewt_widget.hpp>
#include <scwx/qt/view/hodograph_widget.hpp>
#include <scwx/qt/ui/sounding_parameters_widget.hpp>
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
   explicit SoundingPanelImpl(SoundingPanel* self) : self_(self) {}
   ~SoundingPanelImpl() = default;

   SoundingPanelImpl(const SoundingPanelImpl&)            = delete;
   SoundingPanelImpl& operator=(const SoundingPanelImpl&) = delete;
   SoundingPanelImpl(SoundingPanelImpl&&)                 = delete;
   SoundingPanelImpl& operator=(SoundingPanelImpl&&)      = delete;

   void SetupUi(QWidget* widget)
   {
      static constexpr double kMaxLatitude      = 90.0;
      static constexpr double kMaxLongitude     = 180.0;
      static constexpr double kDefaultLatitude  = 35.0;
      static constexpr double kDefaultLongitude = -97.0;
      static constexpr double kCoordinateStep   = 0.1;
      static constexpr int    kMaxForecastHour  = 384;
      static constexpr int    kForecastStep     = 3;

      static constexpr int kMargin         = 6;
      static constexpr int kSpacing        = 4;
      static constexpr int kControlSpacing = 6;

      auto* mainLayout = new QVBoxLayout(widget);
      mainLayout->setContentsMargins(0, 0, 0, 0);
      mainLayout->setSpacing(0);

      // Controls area
      auto* controlsWidget = new QWidget();
      auto* controlsLayout = new QVBoxLayout(controlsWidget);
      controlsLayout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
      controlsLayout->setSpacing(kSpacing);

      // Row 1: Location and Select Point
      auto* row1Layout = new QHBoxLayout();
      row1Layout->setSpacing(kControlSpacing);

      auto* latLabel = new QLabel("Lat:");
      row1Layout->addWidget(latLabel);
      latSpinBox_ = new QDoubleSpinBox();
      latSpinBox_->setRange(-kMaxLatitude, kMaxLatitude);
      latSpinBox_->setDecimals(4);
      latSpinBox_->setValue(kDefaultLatitude);
      latSpinBox_->setSingleStep(kCoordinateStep);
      static constexpr int kSpinBoxWidth = 80;
      latSpinBox_->setMinimumWidth(kSpinBoxWidth);
      row1Layout->addWidget(latSpinBox_);

      auto* lonLabel = new QLabel("Lon:");
      row1Layout->addWidget(lonLabel);
      lonSpinBox_ = new QDoubleSpinBox();
      lonSpinBox_->setRange(-kMaxLongitude, kMaxLongitude);
      lonSpinBox_->setDecimals(4);
      lonSpinBox_->setValue(kDefaultLongitude);
      lonSpinBox_->setSingleStep(kCoordinateStep);
      lonSpinBox_->setMinimumWidth(kSpinBoxWidth);
      row1Layout->addWidget(lonSpinBox_);

      selectPointButton_ = new QPushButton("Select Point");
      selectPointButton_->setCheckable(true);
      selectPointButton_->setToolTip(
         "Click this button, then click a location on the map to select "
         "forecast coordinates");
      row1Layout->addWidget(selectPointButton_);

      controlsLayout->addLayout(row1Layout);

      // Row 2: Model parameters and Fetch
      auto* row2Layout = new QHBoxLayout();
      row2Layout->setSpacing(kControlSpacing);

      row2Layout->addWidget(new QLabel("Cycle:"));
      cycleCombo_                   = new QComboBox();
      static constexpr int kCycle00 = 0;
      static constexpr int kCycle06 = 6;
      static constexpr int kCycle12 = 12;
      static constexpr int kCycle18 = 18;
      cycleCombo_->addItem("00Z", kCycle00);
      cycleCombo_->addItem("06Z", kCycle06);
      cycleCombo_->addItem("12Z", kCycle12);
      cycleCombo_->addItem("18Z", kCycle18);
      row2Layout->addWidget(cycleCombo_);

      row2Layout->addWidget(new QLabel("Fhr:"));
      fhrCombo_ = new QComboBox();
      static constexpr int kFhrPadWidth = 3;
      static constexpr int kBaseDecimal = 10;
      for (int f = 0; f <= kMaxForecastHour; f += kForecastStep)
      {
         fhrCombo_->addItem(
            QString("F%1").arg(f, kFhrPadWidth, kBaseDecimal, QChar('0')), f);
      }
      fhrCombo_->setCurrentIndex(0); // F000 (analysis)
      row2Layout->addWidget(fhrCombo_);

      fetchButton_ = new QPushButton("Fetch Sounding");
      fetchButton_->setStyleSheet("font-weight: bold;");
      static constexpr int kStretchFetch = 1;
      row2Layout->addWidget(fetchButton_, kStretchFetch);

      controlsLayout->addLayout(row2Layout);

      mainLayout->addWidget(controlsWidget);

      // Add a thin separator
      auto* separator = new QFrame();
      separator->setFrameShape(QFrame::HLine);
      separator->setFrameShadow(QFrame::Sunken);
      separator->setStyleSheet("background-color: #333;");
      mainLayout->addWidget(separator);

      // Plots and Parameters
      auto* hSplitter = new QSplitter(Qt::Horizontal);
      auto* vSplitter = new QSplitter(Qt::Vertical);

      skewtWidget_     = new view::SkewtWidget();
      hodographWidget_ = new view::HodographWidget();

      static constexpr int kIndexSkewT       = 0;
      static constexpr int kIndexHodograph   = 1;
      static constexpr int kStretchSkewT     = 3;
      static constexpr int kStretchHodograph = 2;
      vSplitter->addWidget(skewtWidget_);
      vSplitter->addWidget(hodographWidget_);
      vSplitter->setStretchFactor(kIndexSkewT, kStretchSkewT);
      vSplitter->setStretchFactor(kIndexHodograph, kStretchHodograph);

      parametersWidget_ = new SoundingParametersWidget();

      static constexpr int kIndexPlots        = 0;
      static constexpr int kIndexParameters   = 1;
      static constexpr int kStretchPlots      = 3;
      static constexpr int kStretchParameters = 1;
      hSplitter->addWidget(vSplitter);
      hSplitter->addWidget(parametersWidget_);
      hSplitter->setStretchFactor(kIndexPlots, kStretchPlots);
      hSplitter->setStretchFactor(kIndexParameters, kStretchParameters);

      mainLayout->addWidget(hSplitter, 1);

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

      // Connect widget synchronization
      QObject::connect(skewtWidget_,
                       &view::SkewtWidget::LevelHovered,
                       hodographWidget_,
                       &view::HodographWidget::SetHoverLevel);
      QObject::connect(hodographWidget_,
                       &view::HodographWidget::LevelHovered,
                       skewtWidget_,
                       &view::SkewtWidget::SetHoverLevel);
   }

   SoundingPanel*            self_;
   view::SkewtWidget*        skewtWidget_ {nullptr};
   view::HodographWidget*    hodographWidget_ {nullptr};
   SoundingParametersWidget* parametersWidget_ {nullptr};
   QDoubleSpinBox*           latSpinBox_ {nullptr};
   QDoubleSpinBox*           lonSpinBox_ {nullptr};
   QComboBox*                cycleCombo_ {nullptr};
   QComboBox*                fhrCombo_ {nullptr};
   QPushButton*              selectPointButton_ {nullptr};
   QPushButton*              fetchButton_ {nullptr};
};

SoundingPanel::SoundingPanel(QWidget* parent) :
    QDockWidget("GFS Sounding", parent),
    p(std::make_unique<SoundingPanelImpl>(this))
{
   setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
   setFeatures(QDockWidget::DockWidgetMovable |
               QDockWidget::DockWidgetFloatable |
               QDockWidget::DockWidgetClosable);
   static constexpr int kMinimumWidth = 400;
   setMinimumWidth(kMinimumWidth);

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
   const std::shared_ptr<sounding::SoundingData>& sounding)
{
   p->skewtWidget_->SetSounding(sounding);
   p->hodographWidget_->SetSounding(sounding);
   p->parametersWidget_->SetSounding(sounding);
   p->fetchButton_->setEnabled(true);
   p->fetchButton_->setText("Fetch Sounding");
   p->selectPointButton_->setChecked(false);
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
