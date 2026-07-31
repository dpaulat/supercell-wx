#include <scwx/qt/ui/model_widget.hpp>

#include <scwx/qt/manager/model_manager.hpp>
#include <scwx/qt/map/map_widget.hpp>

#include <algorithm>
#include <array>

#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace scwx::qt::ui
{
namespace
{

QString FormatForecastHours(const QVector<int>& hours)
{
   QStringList parts;
   for (int begin = 0; begin < hours.size();)
   {
      int end = begin;
      while (end + 1 < hours.size() && hours[end + 1] == hours[end] + 1)
         ++end;
      if (end - begin >= 2)
      {
         parts.push_back(QString("%1-%2").arg(hours[begin]).arg(hours[end]));
      }
      else
      {
         for (int i = begin; i <= end; ++i)
            parts.push_back(QString::number(hours[i]));
      }
      begin = end + 1;
   }
   return parts.join(',');
}

// The model controls live inside the radar toolbox scroll area. Combo boxes,
// spin boxes and sliders normally consume wheel events merely because the
// pointer is over them, which makes a long toolbox difficult to navigate and
// can accidentally change a run. Keep the wheel dedicated to sidebar
// navigation; the controls remain fully usable by click, drag and keyboard.
class SidebarWheelFilter final : public QObject
{
public:
   using QObject::QObject;

protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
      if (event->type() != QEvent::Wheel)
         return QObject::eventFilter(watched, event);

      auto* widget = qobject_cast<QWidget*>(watched);
      auto* wheel  = static_cast<QWheelEvent*>(event);
      if (widget == nullptr)
         return QObject::eventFilter(watched, event);

      for (QWidget* ancestor = widget->parentWidget(); ancestor != nullptr;
           ancestor          = ancestor->parentWidget())
      {
         auto* scrollArea = qobject_cast<QAbstractScrollArea*>(ancestor);
         if (scrollArea == nullptr)
            continue;

         auto* scrollBar = scrollArea->verticalScrollBar();
         int   distance  = wheel->pixelDelta().y();
         if (distance == 0 && wheel->angleDelta().y() != 0)
         {
            const int steps = wheel->angleDelta().y() / 120;
            distance =
               (steps == 0 ? (wheel->angleDelta().y() > 0 ? 1 : -1) : steps) *
               std::max(scrollBar->singleStep() * 3, 24);
         }
         if (wheel->inverted())
            distance = -distance;
         scrollBar->setValue(scrollBar->value() - distance);
         wheel->accept();
         return true;
      }

      return QObject::eventFilter(watched, event);
   }
};

class SoundingWindow final : public QDialog
{
public:
   explicit SoundingWindow(QWidget* parent) : QDialog(parent, Qt::Window)
   {
      setWindowTitle(tr("Model Sounding"));
      setModal(false);
      resize(1400, 900);

      auto* layout  = new QVBoxLayout(this);
      auto* toolbar = new QHBoxLayout();
      title_        = new QLabel(this);
      title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
      fit_ = new QCheckBox(tr("Fit to window"), this);
      fit_->setChecked(true);
      toolbar->addWidget(title_, 1);
      toolbar->addWidget(fit_);
      layout->addLayout(toolbar);

      scroll_ = new QScrollArea(this);
      scroll_->setAlignment(Qt::AlignCenter);
      scroll_->setWidgetResizable(true);
      image_ = new QLabel(scroll_);
      image_->setAlignment(Qt::AlignCenter);
      scroll_->setWidget(image_);
      scroll_->viewport()->installEventFilter(this);
      layout->addWidget(scroll_, 1);

      QObject::connect(
         fit_, &QCheckBox::toggled, this, [this]() { UpdateImage(); });
   }

   void SetSounding(const QPixmap& image, const QString& title)
   {
      source_ = image;
      title_->setText(title);
      UpdateImage();
   }

protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
      if (watched == scroll_->viewport() && event->type() == QEvent::Resize &&
          fit_->isChecked())
      {
         UpdateImage();
      }
      return QDialog::eventFilter(watched, event);
   }

private:
   void UpdateImage()
   {
      if (source_.isNull())
         return;

      if (fit_->isChecked())
      {
         scroll_->setWidgetResizable(true);
         const QSize available = scroll_->viewport()->size();
         image_->resize(available);
         image_->setPixmap(source_.scaled(
            available, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      }
      else
      {
         scroll_->setWidgetResizable(false);
         image_->setPixmap(source_);
         image_->resize(source_.size());
      }
   }

   QLabel*      title_ {};
   QCheckBox*   fit_ {};
   QScrollArea* scroll_ {};
   QLabel*      image_ {};
   QPixmap      source_ {};
};

} // namespace

class ModelWidget::Impl
{
public:
   explicit Impl(ModelWidget* self) : self_ {self} {}

   void BuildUi();
   void ConnectSignals();
   void PopulateModels(const QVector<types::ForecastModel>& models);
   void PopulateProducts(const QVector<types::ModelProduct>& products);
   void FilterProducts();
   void UpdateModelDetails();
   void Render();
   void GenerateSounding();
   void UseMapCenter();
   void Step(int delta);

   ModelWidget*                           self_;
   map::MapWidget*                        mapWidget_ {nullptr};
   std::shared_ptr<manager::ModelManager> manager_ {
      manager::ModelManager::Instance()};
   QVector<types::ForecastModel> models_;
   QVector<types::ModelProduct>  products_;
   QVector<int>                  storedHours_;
   QString                       currentModel_;
   QString                       currentRun_;
   QString                       currentSource_;

   QLineEdit*                     bridgePath_ {};
   QLabel*                        status_ {};
   QLabel*                        updated_ {};
   QComboBox*                     model_ {};
   QComboBox*                     source_ {};
   QComboBox*                     run_ {};
   QDateEdit*                     date_ {};
   QComboBox*                     cycle_ {};
   QSpinBox*                      probeHour_ {};
   QLineEdit*                     hours_ {};
   QComboBox*                     profile_ {};
   QCheckBox*                     heavy_ {};
   QCheckBox*                     verify_ {};
   QPushButton*                   refresh_ {};
   QPushButton*                   probe_ {};
   QPushButton*                   fetch_ {};
   QPushButton*                   storedRuns_ {};
   QPushButton*                   cancel_ {};
   QLineEdit*                     search_ {};
   QCheckBox*                     favoritesOnly_ {};
   QListWidget*                   productList_ {};
   QPushButton*                   favorite_ {};
   QCheckBox*                     currentView_ {};
   std::array<QDoubleSpinBox*, 4> bounds_ {};
   QPushButton*                   render_ {};
   QSlider*                       frame_ {};
   QLabel*                        frameLabel_ {};
   QToolButton*                   previous_ {};
   QToolButton*                   play_ {};
   QToolButton*                   next_ {};
   QTimer                         playTimer_;
   QCheckBox*                     timelineSync_ {};
   QCheckBox*                     visible_ {};
   QSlider*                       opacity_ {};
   QSpinBox*                      soundingHour_ {};
   QDoubleSpinBox*                soundingLatitude_ {};
   QDoubleSpinBox*                soundingLongitude_ {};
   QPushButton*                   useMapCenter_ {};
   QPushButton*                   sounding_ {};
   QPushButton*                   openSounding_ {};
   QLabel*                        soundingStatus_ {};
   QLabel*                        soundingImage_ {};
   SoundingWindow*                soundingWindow_ {};
   QProgressBar*                  progress_ {};
   int                            pendingCatalogHour_ {-1};
   bool                           busy_ {false};
};

ModelWidget::ModelWidget(QWidget* parent) :
    QWidget(parent), p(std::make_unique<Impl>(this))
{
   p->BuildUi();
   p->ConnectSignals();
   p->manager_->LoadCapabilities();
}

ModelWidget::~ModelWidget() = default;

void ModelWidget::SetMapWidget(map::MapWidget* mapWidget)
{
   p->mapWidget_ = mapWidget;
   p->UseMapCenter();
}

void ModelWidget::Impl::BuildUi()
{
   auto* root = new QVBoxLayout(self_);
   root->setContentsMargins(0, 0, 0, 0);
   self_->setMinimumWidth(0);
   self_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

   auto*      wheelFilter  = new SidebarWheelFilter(self_);
   const auto compactCombo = [wheelFilter](QComboBox* combo)
   {
      combo->setSizeAdjustPolicy(
         QComboBox::AdjustToMinimumContentsLengthWithIcon);
      combo->setMinimumContentsLength(8);
      combo->setMinimumWidth(0);
      combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
      combo->installEventFilter(wheelFilter);
   };

   auto* bridgeGroup  = new QGroupBox(self_->tr("Processor"), self_);
   auto* bridgeLayout = new QGridLayout(bridgeGroup);
   bridgePath_        = new QLineEdit(manager_->bridge_path(), bridgeGroup);
   bridgePath_->setMinimumWidth(0);
   bridgePath_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
   auto* browse = new QPushButton(self_->tr("Browse…"), bridgeGroup);
   refresh_     = new QPushButton(self_->tr("Reload catalog"), bridgeGroup);
   status_      = new QLabel(self_->tr("Not loaded"), bridgeGroup);
   updated_     = new QLabel(bridgeGroup);
   status_->setWordWrap(true);
   updated_->setWordWrap(true);
   status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
   updated_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
   updated_->setTextInteractionFlags(Qt::TextSelectableByMouse);
   bridgeLayout->addWidget(bridgePath_, 0, 0, 1, 2);
   bridgeLayout->addWidget(browse, 0, 2);
   bridgeLayout->addWidget(refresh_, 1, 0);
   bridgeLayout->addWidget(status_, 1, 1, 1, 2);
   bridgeLayout->addWidget(updated_, 2, 0, 1, 3);
   root->addWidget(bridgeGroup);

   auto* runGroup = new QGroupBox(self_->tr("Run and download"), self_);
   auto* runForm  = new QFormLayout(runGroup);
   model_         = new QComboBox(runGroup);
   source_        = new QComboBox(runGroup);
   run_           = new QComboBox(runGroup);
   date_ = new QDateEdit(QDateTime::currentDateTimeUtc().date(), runGroup);
   date_->setCalendarPopup(true);
   cycle_     = new QComboBox(runGroup);
   probeHour_ = new QSpinBox(runGroup);
   probeHour_->setRange(0, 999);
   hours_ = new QLineEdit("0", runGroup);
   hours_->setToolTip(self_->tr(
      "Forecast hours to download or render. Examples: 0, 0,3,6, or 0-48"));
   profile_ = new QComboBox(runGroup);
   profile_->addItems({"view", "full", "sounding"});
   compactCombo(model_);
   compactCombo(source_);
   compactCombo(run_);
   compactCombo(cycle_);
   compactCombo(profile_);
   date_->installEventFilter(wheelFilter);
   probeHour_->installEventFilter(wheelFilter);
   heavy_ = new QCheckBox(self_->tr("Heavy ECAPE"), runGroup);
   heavy_->setToolTip(self_->tr(
      "Adds CPU-intensive ECAPE diagnostics. HRRR commonly takes 1-2 "
      "minutes per hour; leave this off for normal full processing."));
   verify_ = new QCheckBox(self_->tr("Verify stored output"), runGroup);
   auto* runButtons       = new QWidget(runGroup);
   auto* runButtonsLayout = new QGridLayout(runButtons);
   runButtonsLayout->setContentsMargins(0, 0, 0, 0);
   probe_      = new QPushButton(self_->tr("Find latest"), runButtons);
   fetch_      = new QPushButton(self_->tr("Download / process"), runButtons);
   storedRuns_ = new QPushButton(self_->tr("Stored runs"), runButtons);
   cancel_     = new QPushButton(self_->tr("Cancel"), runButtons);
   cancel_->setEnabled(false);
   runButtonsLayout->addWidget(probe_, 0, 0);
   runButtonsLayout->addWidget(fetch_, 0, 1);
   runButtonsLayout->addWidget(storedRuns_, 1, 0);
   runButtonsLayout->addWidget(cancel_, 1, 1);
   runForm->addRow(self_->tr("Model"), model_);
   runForm->addRow(self_->tr("Source"), source_);
   runForm->addRow(self_->tr("Local run"), run_);
   runForm->addRow(self_->tr("UTC date"), date_);
   runForm->addRow(self_->tr("Cycle"), cycle_);
   runForm->addRow(self_->tr("Probe hour"), probeHour_);
   runForm->addRow(self_->tr("Forecast hours"), hours_);
   runForm->addRow(self_->tr("Data profile"), profile_);
   runForm->addRow(heavy_);
   runForm->addRow(verify_);
   runForm->addRow(runButtons);
   root->addWidget(runGroup);

   auto* tabs      = new QTabWidget(self_);
   auto* mapPage   = new QWidget(tabs);
   auto* mapLayout = new QVBoxLayout(mapPage);
   mapLayout->setContentsMargins(0, 0, 0, 0);

   auto* productGroup  = new QGroupBox(self_->tr("Products"), mapPage);
   auto* productLayout = new QVBoxLayout(productGroup);
   search_             = new QLineEdit(productGroup);
   search_->setPlaceholderText(self_->tr("Search products…"));
   favoritesOnly_ = new QCheckBox(self_->tr("Favorites only"), productGroup);
   productList_   = new QListWidget(productGroup);
   productList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
   productList_->setMinimumHeight(120);
   productList_->setMaximumHeight(180);
   productList_->setMinimumWidth(0);
   productList_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
   favorite_ = new QPushButton(self_->tr("Toggle favorite"), productGroup);
   render_ =
      new QPushButton(self_->tr("Render current viewport"), productGroup);
   render_->setEnabled(false);
   render_->setToolTip(
      self_->tr("Download or load a run, select one or more products, then "
                "display them"));
   productLayout->addWidget(search_);
   productLayout->addWidget(favoritesOnly_);
   productLayout->addWidget(productList_);
   productLayout->addWidget(favorite_);
   productLayout->addWidget(render_);
   mapLayout->addWidget(productGroup);

   auto* displayGroup  = new QGroupBox(self_->tr("Map overlay"), mapPage);
   auto* displayLayout = new QGridLayout(displayGroup);
   currentView_ =
      new QCheckBox(self_->tr("Use current viewport bounds"), displayGroup);
   currentView_->setChecked(true);
   currentView_->setToolTip(self_->tr(
      "Creates a fixed raster snapshot of the current view. Re-render after "
      "panning or zooming outside those bounds."));
   const std::array<double, 4>  defaults {-130.0, -60.0, 20.0, 55.0};
   const std::array<QString, 4> labels {self_->tr("West"),
                                        self_->tr("East"),
                                        self_->tr("South"),
                                        self_->tr("North")};
   for (int i = 0; i < 4; ++i)
   {
      bounds_[i] = new QDoubleSpinBox(displayGroup);
      bounds_[i]->setRange(i < 2 ? -180.0 : -85.0, i < 2 ? 180.0 : 85.0);
      bounds_[i]->setDecimals(3);
      bounds_[i]->setValue(defaults[i]);
      bounds_[i]->setEnabled(false);
      displayLayout->addWidget(
         new QLabel(labels[i], displayGroup), 1 + i / 2, (i % 2) * 2);
      displayLayout->addWidget(bounds_[i], 1 + i / 2, (i % 2) * 2 + 1);
   }
   visible_ = new QCheckBox(self_->tr("Show on map"), displayGroup);
   visible_->setChecked(manager_->visible());
   opacity_ = new QSlider(Qt::Horizontal, displayGroup);
   opacity_->setRange(0, 100);
   opacity_->setValue(static_cast<int>(manager_->opacity() * 100.0f));
   opacity_->installEventFilter(wheelFilter);
   timelineSync_ =
      new QCheckBox(self_->tr("Sync with main timeline"), displayGroup);
   timelineSync_->setChecked(manager_->timeline_sync());
   displayLayout->addWidget(currentView_, 0, 0, 1, 4);
   displayLayout->addWidget(visible_, 3, 0);
   displayLayout->addWidget(
      new QLabel(self_->tr("Opacity"), displayGroup), 3, 1);
   displayLayout->addWidget(opacity_, 3, 2, 1, 2);
   displayLayout->addWidget(timelineSync_, 4, 0, 1, 4);
   mapLayout->addWidget(displayGroup);

   auto* timelineGroup = new QGroupBox(self_->tr("Forecast timeline"), mapPage);
   auto* timelineLayout = new QGridLayout(timelineGroup);
   frame_               = new QSlider(Qt::Horizontal, timelineGroup);
   frame_->setRange(0, 0);
   frame_->installEventFilter(wheelFilter);
   frameLabel_ = new QLabel("F000", timelineGroup);
   previous_   = new QToolButton(timelineGroup);
   previous_->setText("◀");
   play_ = new QToolButton(timelineGroup);
   play_->setText("▶");
   play_->setCheckable(true);
   next_ = new QToolButton(timelineGroup);
   next_->setText("▶|");
   timelineLayout->addWidget(frame_, 0, 0, 1, 3);
   timelineLayout->addWidget(frameLabel_, 0, 3);
   timelineLayout->addWidget(previous_, 1, 0);
   timelineLayout->addWidget(play_, 1, 1);
   timelineLayout->addWidget(next_, 1, 2);
   mapLayout->addWidget(timelineGroup);
   mapLayout->addStretch();
   tabs->addTab(mapPage, self_->tr("Map"));

   auto* soundingPage   = new QWidget(tabs);
   auto* soundingLayout = new QVBoxLayout(soundingPage);
   soundingLayout->setContentsMargins(0, 0, 0, 0);
   auto* pointGroup = new QGroupBox(self_->tr("Point sounding"), soundingPage);
   auto* pointForm  = new QFormLayout(pointGroup);
   soundingHour_    = new QSpinBox(pointGroup);
   soundingHour_->setRange(0, 999);
   soundingHour_->installEventFilter(wheelFilter);
   soundingLatitude_ = new QDoubleSpinBox(pointGroup);
   soundingLatitude_->setRange(-90.0, 90.0);
   soundingLatitude_->setDecimals(4);
   soundingLatitude_->setValue(35.0);
   soundingLatitude_->installEventFilter(wheelFilter);
   soundingLongitude_ = new QDoubleSpinBox(pointGroup);
   soundingLongitude_->setRange(-180.0, 180.0);
   soundingLongitude_->setDecimals(4);
   soundingLongitude_->setValue(-97.0);
   soundingLongitude_->installEventFilter(wheelFilter);
   useMapCenter_ = new QPushButton(self_->tr("Use map center"), pointGroup);
   sounding_     = new QPushButton(self_->tr("Generate sounding"), pointGroup);
   sounding_->setEnabled(false);
   openSounding_ =
      new QPushButton(self_->tr("Open sounding in window"), pointGroup);
   openSounding_->setEnabled(false);
   auto* soundingButtons       = new QWidget(pointGroup);
   auto* soundingButtonsLayout = new QGridLayout(soundingButtons);
   soundingButtonsLayout->setContentsMargins(0, 0, 0, 0);
   soundingButtonsLayout->addWidget(useMapCenter_, 0, 0);
   soundingButtonsLayout->addWidget(sounding_, 0, 1);
   soundingButtonsLayout->addWidget(openSounding_, 1, 0, 1, 2);
   pointForm->addRow(self_->tr("Forecast hour"), soundingHour_);
   pointForm->addRow(self_->tr("Latitude"), soundingLatitude_);
   pointForm->addRow(self_->tr("Longitude"), soundingLongitude_);
   pointForm->addRow(soundingButtons);
   soundingLayout->addWidget(pointGroup);

   soundingStatus_ = new QLabel(
      self_->tr("Download a run with the sounding profile, then generate a "
                "sounding at any point."),
      soundingPage);
   soundingStatus_->setWordWrap(true);
   soundingStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
   soundingLayout->addWidget(soundingStatus_);

   auto* soundingScroll = new QScrollArea(soundingPage);
   soundingScroll->setWidgetResizable(false);
   soundingScroll->setAlignment(Qt::AlignCenter);
   soundingScroll->setMinimumHeight(360);
   soundingImage_ = new QLabel(soundingScroll);
   soundingImage_->setAlignment(Qt::AlignCenter);
   soundingImage_->setText(self_->tr("No sounding generated"));
   soundingImage_->resize(480, 320);
   soundingScroll->setWidget(soundingImage_);
   soundingLayout->addWidget(soundingScroll, 1);
   soundingWindow_ = new SoundingWindow(self_);
   tabs->addTab(soundingPage, self_->tr("Sounding"));
   root->addWidget(tabs, 1);

   QObject::connect(tabs,
                    &QTabWidget::currentChanged,
                    self_,
                    [this](int index)
                    {
                       if (index == 1 && profile_->currentText() == "view")
                          profile_->setCurrentText("sounding");
                    });

   progress_ = new QProgressBar(self_);
   progress_->setRange(0, 1);
   progress_->setValue(0);
   progress_->setTextVisible(true);
   root->addWidget(progress_);

   QObject::connect(browse,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       const QString path = QFileDialog::getOpenFileName(
                          self_,
                          self_->tr("Select Rusty Weather model bridge"));
                       if (!path.isEmpty())
                       {
                          bridgePath_->setText(path);
                          manager_->SetBridgePath(path);
                       }
                    });
}

void ModelWidget::Impl::ConnectSignals()
{
   QObject::connect(refresh_,
                    &QPushButton::clicked,
                    manager_.get(),
                    &manager::ModelManager::LoadCapabilities);
   QObject::connect(bridgePath_,
                    &QLineEdit::editingFinished,
                    self_,
                    [this]() { manager_->SetBridgePath(bridgePath_->text()); });
   QObject::connect(model_,
                    qOverload<int>(&QComboBox::currentIndexChanged),
                    self_,
                    [this](int) { UpdateModelDetails(); });
   QObject::connect(probe_,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       manager_->Probe(model_->currentData().toString(),
                                       date_->date(),
                                       probeHour_->value(),
                                       source_->currentData().toString());
                    });
   QObject::connect(fetch_,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       manager_->Fetch(model_->currentData().toString(),
                                       date_->date().toString("yyyyMMdd"),
                                       cycle_->currentData().toInt(),
                                       hours_->text(),
                                       source_->currentData().toString(),
                                       profile_->currentText(),
                                       heavy_->isChecked(),
                                       verify_->isChecked());
                    });
   QObject::connect(storedRuns_,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    { manager_->ListRuns(model_->currentData().toString()); });
   QObject::connect(
      run_,
      qOverload<int>(&QComboBox::currentIndexChanged),
      self_,
      [this](int index)
      {
         if (index < 0)
            return;
         currentModel_  = model_->currentData().toString();
         currentRun_    = run_->itemData(index, Qt::UserRole).toString();
         currentSource_ = source_->currentData().toString();
         const auto hourStrings =
            run_->itemData(index, Qt::UserRole + 1).toStringList();
         if (currentRun_.isEmpty() || hourStrings.isEmpty())
            return;
         storedHours_.clear();
         for (const auto& hour : hourStrings)
         {
            storedHours_.push_back(hour.toInt());
         }
         hours_->setText(hourStrings.join(','));
         manager_->Catalog(currentModel_, currentRun_, storedHours_.front());
      });
   QObject::connect(cancel_,
                    &QPushButton::clicked,
                    manager_.get(),
                    &manager::ModelManager::Cancel);
   QObject::connect(
      search_, &QLineEdit::textChanged, self_, [this]() { FilterProducts(); });
   QObject::connect(favoritesOnly_,
                    &QCheckBox::toggled,
                    self_,
                    [this]() { FilterProducts(); });
   QObject::connect(favorite_,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       auto* item = productList_->currentItem();
                       if (item == nullptr)
                          return;
                       const QString id = item->data(Qt::UserRole).toString();
                       const bool    favorite =
                          !manager_->favorites().contains(id);
                       manager_->SetFavorite(id, favorite);
                       PopulateProducts(products_);
                    });
   QObject::connect(productList_,
                    &QListWidget::itemSelectionChanged,
                    self_,
                    [this]()
                    {
                       render_->setEnabled(
                          !busy_ && !currentRun_.isEmpty() &&
                          !storedHours_.isEmpty() &&
                          !productList_->selectedItems().isEmpty());
                    });
   QObject::connect(productList_,
                    &QListWidget::itemDoubleClicked,
                    self_,
                    [this](QListWidgetItem*)
                    {
                       if (render_->isEnabled())
                          Render();
                    });
   QObject::connect(currentView_,
                    &QCheckBox::toggled,
                    self_,
                    [this](bool current)
                    {
                       for (auto* bound : bounds_)
                          bound->setEnabled(!current);
                       render_->setText(
                          current ? self_->tr("Render current viewport") :
                                    self_->tr("Render custom bounds"));
                    });
   QObject::connect(
      render_, &QPushButton::clicked, self_, [this]() { Render(); });
   QObject::connect(useMapCenter_,
                    &QPushButton::clicked,
                    self_,
                    [this]() { UseMapCenter(); });
   QObject::connect(sounding_,
                    &QPushButton::clicked,
                    self_,
                    [this]() { GenerateSounding(); });
   QObject::connect(openSounding_,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       soundingWindow_->show();
                       soundingWindow_->raise();
                       soundingWindow_->activateWindow();
                    });
   QObject::connect(visible_,
                    &QCheckBox::toggled,
                    manager_.get(),
                    &manager::ModelManager::SetVisible);
   QObject::connect(
      opacity_,
      &QSlider::valueChanged,
      self_,
      [this](int value)
      { manager_->SetOpacity(static_cast<float>(value) / 100.0f); });
   QObject::connect(timelineSync_,
                    &QCheckBox::toggled,
                    manager_.get(),
                    &manager::ModelManager::SetTimelineSync);
   QObject::connect(
      frame_,
      &QSlider::valueChanged,
      self_,
      [this](int index)
      {
         if (index >= 0 && index < static_cast<int>(storedHours_.size()))
         {
            const int hour = storedHours_[index];
            frameLabel_->setText(QString("F%1").arg(hour, 3, 10, QChar('0')));
            soundingHour_->setValue(hour);
            manager_->SelectForecastHour(hour);
         }
      });
   QObject::connect(
      previous_, &QToolButton::clicked, self_, [this]() { Step(-1); });
   QObject::connect(next_, &QToolButton::clicked, self_, [this]() { Step(1); });
   playTimer_.setInterval(750);
   QObject::connect(
      &playTimer_, &QTimer::timeout, self_, [this]() { Step(1); });
   QObject::connect(play_,
                    &QToolButton::toggled,
                    self_,
                    [this](bool playing)
                    {
                       play_->setText(playing ? "Ⅱ" : "▶");
                       if (playing)
                          playTimer_.start();
                       else
                          playTimer_.stop();
                    });

   QObject::connect(manager_.get(),
                    &manager::ModelManager::CapabilitiesUpdated,
                    self_,
                    [this](const auto& models) { PopulateModels(models); });
   QObject::connect(
      manager_.get(),
      &manager::ModelManager::ProbeCompleted,
      self_,
      [this](const types::ModelProbeResult& result)
      {
         date_->setDate(QDate::fromString(result.date_, "yyyyMMdd"));
         const int cycleIndex = cycle_->findData(result.cycleUtc_);
         if (cycleIndex >= 0)
            cycle_->setCurrentIndex(cycleIndex);
         const int sourceIndex = source_->findData(result.source_);
         if (sourceIndex >= 0)
            source_->setCurrentIndex(sourceIndex);
         if (!result.forecastHours_.isEmpty())
         {
            hours_->setToolTip(
               self_
                  ->tr("Available forecast hours: %1. The value in this field "
                       "controls which hours will be downloaded or rendered.")
                  .arg(FormatForecastHours(result.forecastHours_)));
         }
      });
   QObject::connect(manager_.get(),
                    &manager::ModelManager::FetchCompleted,
                    self_,
                    [this](const QString&      model,
                           const QString&      run,
                           const QString&      source,
                           const QVector<int>& hours)
                    {
                       currentModel_  = model;
                       currentRun_    = run;
                       currentSource_ = source;
                       if (!hours.isEmpty())
                          pendingCatalogHour_ = hours.front();
                    });
   QObject::connect(manager_.get(),
                    &manager::ModelManager::RunsUpdated,
                    self_,
                    [this](const types::ModelRuns& result)
                    {
                       const QSignalBlocker blocker {run_};
                       run_->clear();
                       currentModel_.clear();
                       currentRun_.clear();
                       currentSource_.clear();
                       storedHours_.clear();
                       pendingCatalogHour_ = -1;
                       frame_->setRange(0, 0);
                       frameLabel_->setText("F000");
                       render_->setEnabled(false);
                       sounding_->setEnabled(false);
                       manager_->SelectForecastHour(-1);
                       for (const auto& stored : result.runs_)
                       {
                          QStringList hours;
                          for (int hour : stored.forecastHours_)
                             hours.push_back(QString::number(hour));
                          run_->addItem(stored.run_);
                          const int index = run_->count() - 1;
                          run_->setItemData(index, stored.run_, Qt::UserRole);
                          run_->setItemData(index, hours, Qt::UserRole + 1);
                       }
                       if (!result.runs_.isEmpty())
                       {
                          currentModel_  = result.model_;
                          currentRun_    = result.runs_.front().run_;
                          currentSource_ = source_->currentData().toString();
                          storedHours_   = result.runs_.front().forecastHours_;
                          QStringList hours;
                          for (int hour : storedHours_)
                             hours.push_back(QString::number(hour));
                          hours_->setText(hours.join(','));
                          pendingCatalogHour_ = storedHours_.front();
                       }
                    });
   QObject::connect(
      manager_.get(),
      &manager::ModelManager::CatalogUpdated,
      self_,
      [this](const types::ModelCatalog& catalog)
      {
         currentModel_ = catalog.model_;
         currentRun_   = catalog.run_;
         storedHours_  = catalog.storedHours_;
         std::sort(storedHours_.begin(), storedHours_.end());
         frame_->setRange(
            0, std::max(0, static_cast<int>(storedHours_.size()) - 1));
         if (!storedHours_.isEmpty())
         {
            soundingHour_->setRange(storedHours_.front(), storedHours_.back());
            soundingHour_->setValue(storedHours_[std::clamp(
               frame_->value(), 0, static_cast<int>(storedHours_.size()) - 1)]);
         }
         auto       products = catalog.products_;
         const auto model =
            std::find_if(models_.cbegin(),
                         models_.cend(),
                         [&catalog](const auto& candidate)
                         { return candidate.id_ == catalog.model_; });
         if (model != models_.cend())
         {
            for (auto& product : products)
            {
               const auto definition =
                  std::find_if(model->products_.cbegin(),
                               model->products_.cend(),
                               [&product](const auto& candidate)
                               { return candidate.id_ == product.id_; });
               if (definition != model->products_.cend())
               {
                  product.title_        = definition->title_;
                  product.heavy_        = definition->heavy_;
                  product.experimental_ = definition->experimental_;
                  product.mapOverlaySupported_ =
                     definition->mapOverlaySupported_;
               }
            }
         }
         PopulateProducts(products);
         render_->setEnabled(!busy_ && !currentRun_.isEmpty() &&
                             !storedHours_.isEmpty() &&
                             !productList_->selectedItems().isEmpty());
         sounding_->setEnabled(!busy_ && !currentRun_.isEmpty() &&
                               !storedHours_.isEmpty());
      });
   QObject::connect(
      manager_.get(),
      &manager::ModelManager::ProgressUpdated,
      self_,
      [this](const QString&, int completed, int total, const QString& message)
      {
         progress_->setRange(0, total > 0 ? total : 0);
         if (total > 0)
            progress_->setValue(completed);
         progress_->setFormat(message);
      });
   QObject::connect(
      manager_.get(),
      &manager::ModelManager::OperationStateChanged,
      self_,
      [this](bool busy, const QString& operation)
      {
         busy_ = busy;
         cancel_->setEnabled(busy);
         refresh_->setEnabled(!busy);
         probe_->setEnabled(!busy);
         fetch_->setEnabled(!busy);
         storedRuns_->setEnabled(!busy);
         render_->setEnabled(!busy && !currentRun_.isEmpty() &&
                             !storedHours_.isEmpty() &&
                             !productList_->selectedItems().isEmpty());
         sounding_->setEnabled(!busy && !currentRun_.isEmpty() &&
                               !storedHours_.isEmpty());
         if (busy)
         {
            status_->setText(self_->tr("Running %1…").arg(operation));
            progress_->setRange(0, 0);
            progress_->setFormat(status_->text());
         }
         if (!busy && pendingCatalogHour_ >= 0)
         {
            const int hour      = pendingCatalogHour_;
            pendingCatalogHour_ = -1;
            manager_->Catalog(currentModel_, currentRun_, hour);
         }
      });
   QObject::connect(
      manager_.get(),
      &manager::ModelManager::SoundingAvailable,
      self_,
      [this](const types::ModelSounding& sounding)
      {
         QPixmap image(sounding.path_);
         if (image.isNull())
         {
            soundingStatus_->setText(
               self_
                  ->tr("The sounding was generated, but its image could not "
                       "be opened: %1")
                  .arg(sounding.path_));
            return;
         }

         soundingImage_->setPixmap(image);
         soundingImage_->resize(image.size());
         QString location = QString("%1 deg, %2 deg")
                               .arg(sounding.selectedLatitude_, 0, 'f', 3)
                               .arg(sounding.selectedLongitude_, 0, 'f', 3);
         if (!sounding.station_.isEmpty())
            location = QString("%1 (%2)").arg(sounding.station_, location);
         soundingStatus_->setText(
            self_->tr("%1 %2 F%3 at %4")
               .arg(sounding.model_.toUpper(),
                    sounding.run_,
                    QString::number(sounding.forecastHour_)
                       .rightJustified(3, '0'),
                    location));
         soundingWindow_->SetSounding(image, soundingStatus_->text());
         openSounding_->setEnabled(true);
      });
   QObject::connect(manager_.get(),
                    &manager::ModelManager::StatusUpdated,
                    self_,
                    [this](const QString& status, const QDateTime& updated)
                    {
                       status_->setText(status);
                       progress_->setRange(0, 1);
                       progress_->setValue(1);
                       progress_->setFormat(status);
                       updated_->setText(self_->tr("Last update: %1")
                                            .arg(QLocale().toString(
                                               updated, QLocale::ShortFormat)));
                    });
   QObject::connect(
      manager_.get(),
      &manager::ModelManager::ErrorOccurred,
      self_,
      [this](const QString& error)
      {
         QString displayError = error;
         if (error.contains("lacks skew-T inputs", Qt::CaseInsensitive))
         {
            displayError =
               self_
                  ->tr(
                     "This stored hour does not include sounding "
                     "fields. Keep Data profile set to sounding, "
                     "include F%1 in Forecast hours, click Download / "
                     "process, then generate the sounding again. "
                     "Cached source files will be reused when "
                     "available.")
                  .arg(soundingHour_->value(), 3, 10, QChar('0'));
            soundingStatus_->setText(displayError);
         }
         status_->setText(displayError);
         progress_->setRange(0, 1);
         progress_->setValue(0);
         progress_->setFormat(displayError);
      });
}

void ModelWidget::Impl::PopulateModels(
   const QVector<types::ForecastModel>& models)
{
   models_ = models;
   model_->clear();
   for (const auto& model : models_)
   {
      if (model.ingestSupported_)
      {
         model_->addItem(model.id_.toUpper(), model.id_);
         model_->setItemData(
            model_->count() - 1, model.description_, Qt::ToolTipRole);
      }
   }
   UpdateModelDetails();
}

void ModelWidget::Impl::UpdateModelDetails()
{
   run_->clear();
   currentModel_.clear();
   currentRun_.clear();
   currentSource_.clear();
   storedHours_.clear();
   pendingCatalogHour_ = -1;
   frame_->setRange(0, 0);
   frameLabel_->setText("F000");
   render_->setEnabled(false);
   sounding_->setEnabled(false);
   manager_->SelectForecastHour(-1);

   const QString id = model_->currentData().toString();
   const auto    found =
      std::find_if(models_.cbegin(),
                   models_.cend(),
                   [&id](const auto& model) { return model.id_ == id; });
   if (found == models_.cend())
      return;
   source_->clear();
   for (const auto& source : found->sources_)
      source_->addItem(source, source);
   cycle_->clear();
   for (int cycle : found->cycleHoursUtc_)
   {
      cycle_->addItem(QString("%1Z").arg(cycle, 2, 10, QChar('0')), cycle);
   }
   probeHour_->setMaximum(found->maxForecastHour_);
   PopulateProducts(found->products_);
}

void ModelWidget::Impl::PopulateProducts(
   const QVector<types::ModelProduct>& products)
{
   QStringList selectedProducts;
   for (const auto* item : productList_->selectedItems())
      selectedProducts.push_back(item->data(Qt::UserRole).toString());

   products_ = products;
   productList_->clear();
   const auto       favorites     = manager_->favorites();
   QListWidgetItem* preferredItem = nullptr;
   QListWidgetItem* firstItem     = nullptr;
   for (const auto& product : products_)
   {
      auto*      item     = new QListWidgetItem(productList_);
      const bool favorite = favorites.contains(product.id_);
      item->setText(
         QString("%1%2 [%3]")
            .arg(favorite ? "★ " : "", product.title_, product.kind_));
      item->setData(Qt::UserRole, product.id_);
      if (!product.mapOverlaySupported_)
      {
         item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
         item->setToolTip(self_->tr(
            "This multi-panel chart cannot be placed on a geographic map"));
      }
      else
      {
         if (firstItem == nullptr)
            firstItem = item;
         if (product.id_ == "composite_reflectivity")
            preferredItem = item;
         item->setToolTip(product.experimental_ ?
                             self_->tr("Experimental product") :
                             product.id_);
      }
      if (selectedProducts.contains(product.id_))
         item->setSelected(true);
   }
   FilterProducts();
   if (productList_->selectedItems().isEmpty())
   {
      auto* selection = preferredItem != nullptr ? preferredItem : firstItem;
      if (selection != nullptr)
      {
         productList_->setCurrentItem(selection);
         selection->setSelected(true);
      }
   }
}

void ModelWidget::Impl::FilterProducts()
{
   const QString needle    = search_->text();
   const auto    favorites = manager_->favorites();
   for (int i = 0; i < productList_->count(); ++i)
   {
      auto*         item = productList_->item(i);
      const QString id   = item->data(Qt::UserRole).toString();
      const bool matches = item->text().contains(needle, Qt::CaseInsensitive) ||
                           id.contains(needle, Qt::CaseInsensitive);
      item->setHidden(!matches ||
                      (favoritesOnly_->isChecked() && !favorites.contains(id)));
   }
}

void ModelWidget::Impl::Render()
{
   QStringList products;
   for (const auto* item : productList_->selectedItems())
   {
      if (item->flags().testFlag(Qt::ItemIsEnabled))
      {
         products.push_back(item->data(Qt::UserRole).toString());
      }
   }
   if (products.isEmpty() && productList_->currentItem() != nullptr &&
       productList_->currentItem()->flags().testFlag(Qt::ItemIsEnabled))
   {
      products.push_back(
         productList_->currentItem()->data(Qt::UserRole).toString());
   }
   if (products.isEmpty())
      return;

   std::array<double, 4> area {bounds_[0]->value(),
                               bounds_[1]->value(),
                               bounds_[2]->value(),
                               bounds_[3]->value()};
   int                   width = 1600;
   if (currentView_->isChecked() && mapWidget_ != nullptr)
   {
      area  = mapWidget_->GetVisibleBounds();
      width = std::clamp(mapWidget_->width() * 2, 800, 4096);
   }
   manager_->Render(currentModel_.isEmpty() ? model_->currentData().toString() :
                                              currentModel_,
                    currentRun_,
                    hours_->text(),
                    products,
                    area[0],
                    area[1],
                    area[2],
                    area[3],
                    width,
                    currentSource_);
}

void ModelWidget::Impl::GenerateSounding()
{
   const QString model = currentModel_.isEmpty() ?
                            model_->currentData().toString() :
                            currentModel_;
   if (model.isEmpty() || currentRun_.isEmpty())
   {
      soundingStatus_->setText(
         self_->tr("Select a stored model run before generating a sounding."));
      return;
   }

   const int hour = soundingHour_->value();
   if (!storedHours_.contains(hour))
   {
      soundingStatus_->setText(
         self_
            ->tr("F%1 is not stored for this run. Choose a stored forecast "
                 "hour or download it first.")
            .arg(hour, 3, 10, QChar('0')));
      return;
   }

   soundingStatus_->setText(
      self_->tr("Generating F%1 sounding...").arg(hour, 3, 10, QChar('0')));
   manager_->Sounding(model,
                      currentRun_,
                      hour,
                      soundingLatitude_->value(),
                      soundingLongitude_->value());
}

void ModelWidget::Impl::UseMapCenter()
{
   if (mapWidget_ == nullptr || soundingLatitude_ == nullptr ||
       soundingLongitude_ == nullptr)
      return;

   double latitude  = 0.0;
   double longitude = 0.0;
   double zoom      = 0.0;
   double bearing   = 0.0;
   double pitch     = 0.0;
   mapWidget_->GetMapViewParameters(latitude, longitude, zoom, bearing, pitch);
   soundingLatitude_->setValue(latitude);
   soundingLongitude_->setValue(longitude);
}

void ModelWidget::Impl::Step(int delta)
{
   if (storedHours_.isEmpty())
      return;
   int       next = frame_->value() + delta;
   const int size = static_cast<int>(storedHours_.size());
   if (next < 0)
      next = size - 1;
   if (next >= size)
      next = 0;
   frame_->setValue(next);
}

} // namespace scwx::qt::ui
