#include <scwx/qt/ui/grib_dock_widget.hpp>
#include <scwx/qt/manager/grib_manager.hpp>
#include <scwx/qt/map/grib_frame_info.hpp>
#include <scwx/qt/ui/checkable_combo_box.hpp>
#include <scwx/qt/ui/widgets/focused_spin_box.hpp>
#include <scwx/provider/rrfs_data_provider.hpp>

#include <chrono>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace scwx::qt::ui
{

namespace
{

std::string CategoryDisplayName(map::GribCategory category)
{
   switch (category)
   {
   case map::GribCategory::Mrms:
      return "MRMS";
   case map::GribCategory::Rtma:
      return "RTMA";
   case map::GribCategory::Rrfs:
   default:
      return "RRFS";
   }
}

// One entry per hourly cycle covers "today so far" at RRFS's real cadence
// (see RrfsDataProvider's own class comment: it really does cycle 24x/day)
// -- a first guess for how far back a run picker should reasonably let
// someone browse, not tied to any documented retention window. Generated
// purely from wall-clock arithmetic (no S3 listing needed just to populate
// the dropdown) -- picking a cycle whose data isn't actually published yet
// just leaves the previously-shown frame in place, same graceful miss
// FetchArchiveFrameForProduct already has for any other out-of-range time.
constexpr int kRrfsCycleHistoryHours_ = 24;

// One forecast-hour step per tick -- a full 84-step loop (a 6-hourly
// cycle's own max) takes ~42s at this pace, fast enough to actually watch
// evolve without being so fast the frame-by-frame detail blurs together.
constexpr int kRrfsAnimationIntervalMs_ = 500;

} // namespace

// One category's worth of UI -- a header, a status label, and the
// checkable "which products are active" combo box.
struct CategorySection
{
   map::GribCategory                     category;
   std::shared_ptr<manager::GribManager> gribManager;
   QLabel*                               statusLabel {};
   CheckableComboBox*                    comboBox {};

   // RRFS-only (left null for Mrms/Rtma, which have no forecast-hour axis
   // -- see GribManager::SetRrfsCycle()'s own doc) -- run/cycle picker,
   // forecast-hour slider, and its play/pause animation loop.
   QComboBox*   cycleComboBox {};
   QSlider*     hourSlider {};
   QLabel*      hourLabel {};
   QPushButton* playButton {};
   QTimer*      animationTimer {};

   // Bounds the Play loop (and, via GribManager::SetRrfsLoopRange(),
   // PrefetchRrfsForecastHourRange()'s own footprint) to a sub-range of
   // [0, MaxRrfsForecastHour()] instead of always the whole cycle -- e.g.
   // "just loop 0-6h" costs proportionally less to prefetch/cache than
   // the full 84h a 6-hourly cycle can reach. Both default to the full
   // range (see BuildSection's own setup), so leaving them alone
   // preserves the original unbounded behavior.
   QSpinBox* loopStartSpinBox {};
   QSpinBox* loopEndSpinBox {};
};

class GribDockWidget::Impl
{
public:
   explicit Impl(GribDockWidget* self) : self_ {self} {}
   ~Impl() = default;

   void BuildSection(map::GribCategory category,
                     QVBoxLayout*      parentLayout,
                     QWidget*          dockContents);
   void RefreshSection(CategorySection& section);

   GribDockWidget*              self_;
   std::vector<CategorySection> sections_;
};

void GribDockWidget::Impl::BuildSection(map::GribCategory category,
                                        QVBoxLayout*      parentLayout,
                                        QWidget*          dockContents)
{
   CategorySection section;
   section.category    = category;
   section.gribManager = manager::GribManager::Instance(category);

   auto* groupBox = new QGroupBox(
      QString::fromStdString(CategoryDisplayName(category)), dockContents);
   auto* groupLayout = new QVBoxLayout(groupBox);

   section.statusLabel = new QLabel(tr("(no frame loaded)"), groupBox);
   section.statusLabel->setWordWrap(true);
   section.statusLabel->setTextInteractionFlags(
      Qt::TextInteractionFlag::TextSelectableByMouse);
   groupLayout->addWidget(section.statusLabel);

   section.comboBox = new CheckableComboBox(groupBox);
   section.comboBox->SetItems(section.gribManager->ProductNames());
   for (const auto& name : section.gribManager->ActiveProductNames())
   {
      section.comboBox->SetChecked(name, true);
   }
   groupLayout->addWidget(section.comboBox);

   if (category == map::GribCategory::Rrfs)
   {
      section.cycleComboBox = new QComboBox(groupBox);
      section.cycleComboBox->addItem(tr("Latest"), QVariant());

      const auto now = std::chrono::floor<std::chrono::hours>(
         std::chrono::system_clock::now());
      for (int i = 0; i < kRrfsCycleHistoryHours_; ++i)
      {
         const auto cycleTime = now - std::chrono::hours {i};
         const int  maxHour =
            provider::RrfsDataProvider::MaxForecastHourForCycle(cycleTime);
         const std::string label = fmt::format(
            "{:%Y-%m-%d %H}z ({}h)", fmt::gmtime(cycleTime), maxHour);
         section.cycleComboBox->addItem(
            QString::fromStdString(label),
            QVariant::fromValue<qint64>(cycleTime.time_since_epoch().count()));
      }
      groupLayout->addWidget(section.cycleComboBox);

      auto* hourRow            = new QHBoxLayout();
      section.hourLabel        = new QLabel(tr("F000"), groupBox);
      section.hourSlider       = new QSlider(Qt::Horizontal, groupBox);
      const int initialMaxHour = section.gribManager->MaxRrfsForecastHour();
      section.hourSlider->setRange(0, initialMaxHour);
      section.playButton = new QPushButton(tr("Play"), groupBox);
      hourRow->addWidget(section.hourLabel);
      hourRow->addWidget(section.hourSlider);
      hourRow->addWidget(section.playButton);
      groupLayout->addLayout(hourRow);

      // Bounds the Play loop to a sub-range instead of always [0,
      // MaxRrfsForecastHour()] -- defaults to the full range (unchanged
      // behavior) until the user narrows it. setRange()'s own clamping
      // keeps start<=end automatically as either spinbox's range is
      // adjusted below.
      auto* loopRow            = new QHBoxLayout();
      auto* loopLabel          = new QLabel(tr("Loop:"), groupBox);
      section.loopStartSpinBox = new QFocusedSpinBox(groupBox);
      section.loopStartSpinBox->setRange(0, initialMaxHour);
      section.loopStartSpinBox->setValue(0);
      auto* loopToLabel      = new QLabel(tr("to"), groupBox);
      section.loopEndSpinBox = new QFocusedSpinBox(groupBox);
      section.loopEndSpinBox->setRange(0, initialMaxHour);
      section.loopEndSpinBox->setValue(initialMaxHour);
      auto* loopUnitsLabel = new QLabel(tr("h"), groupBox);
      loopRow->addWidget(loopLabel);
      loopRow->addWidget(section.loopStartSpinBox);
      loopRow->addWidget(loopToLabel);
      loopRow->addWidget(section.loopEndSpinBox);
      loopRow->addWidget(loopUnitsLabel);
      groupLayout->addLayout(loopRow);

      section.gribManager->SetRrfsLoopRange(0, initialMaxHour);

      section.animationTimer = new QTimer(self_);
      section.animationTimer->setInterval(kRrfsAnimationIntervalMs_);
   }

   parentLayout->addWidget(groupBox);

   // sections_ must already have room for this push_back not to
   // reallocate -- see the constructor's reserve() -- since the lambdas
   // just below capture `stored` (a reference into this vector) for the
   // life of the connection, and a reallocation would dangle every
   // earlier section's capture, not just this one's.
   sections_.push_back(section);
   CategorySection& stored = sections_.back();

   // Reconciles the combo box's checked set onto GribManager rather than
   // trying to diff "what changed" from CheckedItemsChanged alone (which
   // doesn't say which item toggled) -- SetProductActive is a no-op for
   // any product already in the requested state, so this is cheap even
   // though it re-touches every product on every toggle.
   connect(section.comboBox,
           &CheckableComboBox::CheckedItemsChanged,
           self_,
           [&stored]()
           {
              for (const auto& name : stored.gribManager->ProductNames())
              {
                 stored.gribManager->SetProductActive(
                    name, stored.comboBox->IsChecked(name));
              }
           });

   connect(stored.gribManager.get(),
           &manager::GribManager::FrameReady,
           self_,
           [this, &stored](std::size_t) { RefreshSection(stored); });

   if (category == map::GribCategory::Rrfs)
   {
      // Reselecting a cycle re-ranges the hour slider too -- a 3-hourly
      // (non-6-hourly) cycle only reaches F018, not F084, and QSlider
      // clamps the current value into a shrunk range on its own (emitting
      // valueChanged if it had to), so no separate reset is needed here.
      connect(
         stored.cycleComboBox,
         qOverload<int>(&QComboBox::currentIndexChanged),
         self_,
         [&stored](int cycleIndex)
         {
            if (cycleIndex <= 0)
            {
               stored.gribManager->UseLatestRrfsCycle();
            }
            else
            {
               const qint64 ticks =
                  stored.cycleComboBox->itemData(cycleIndex).value<qint64>();
               const auto cycleTime = std::chrono::system_clock::time_point {
                  std::chrono::system_clock::duration {ticks}};
               stored.gribManager->SetRrfsCycle(cycleTime);
            }
            stored.hourSlider->setRange(
               0, stored.gribManager->MaxRrfsForecastHour());

            // Same reasoning as hourSlider's own re-range above -- a
            // shrunk max (e.g. a 3-hourly cycle capping at F018) clamps
            // both spinboxes' current values automatically via Qt's own
            // setRange(); GribManager needs telling explicitly, since it
            // doesn't watch these spinboxes itself.
            const int newMaxHour = stored.gribManager->MaxRrfsForecastHour();
            stored.loopStartSpinBox->setRange(0, newMaxHour);
            stored.loopEndSpinBox->setRange(0, newMaxHour);
            stored.gribManager->SetRrfsLoopRange(
               stored.loopStartSpinBox->value(),
               stored.loopEndSpinBox->value());
         });

      connect(stored.hourSlider,
              &QSlider::valueChanged,
              self_,
              [&stored](int hour)
              {
                 stored.gribManager->SetRrfsForecastHour(hour);
                 stored.hourLabel->setText(
                    QString::fromStdString(fmt::format("F{:03d}", hour)));
              });

      // Cross-clamped so start can never exceed end or vice versa --
      // standard two-spinbox range idiom (each bounds the *other's* own
      // range, not just its value) -- rather than validating and
      // rejecting an inverted range after the fact.
      connect(stored.loopStartSpinBox,
              qOverload<int>(&QSpinBox::valueChanged),
              self_,
              [&stored](int start)
              {
                 stored.loopEndSpinBox->setMinimum(start);
                 stored.gribManager->SetRrfsLoopRange(
                    start, stored.loopEndSpinBox->value());
              });

      connect(stored.loopEndSpinBox,
              qOverload<int>(&QSpinBox::valueChanged),
              self_,
              [&stored](int end)
              {
                 stored.loopStartSpinBox->setMaximum(end);
                 stored.gribManager->SetRrfsLoopRange(
                    stored.loopStartSpinBox->value(), end);
              });

      connect(stored.playButton,
              &QPushButton::clicked,
              self_,
              [&stored]()
              {
                 if (stored.animationTimer->isActive())
                 {
                    stored.animationTimer->stop();
                    stored.playButton->setText(tr("Play"));
                 }
                 else
                 {
                    // Starting outside the loop range (e.g. the slider was
                    // left at F040 from manual scrubbing, then the loop
                    // was narrowed to 0-6h) would otherwise animate
                    // outside the range the user just asked for until it
                    // happened to wrap around into it -- snap in first.
                    const int loopStart = stored.loopStartSpinBox->value();
                    const int loopEnd   = stored.loopEndSpinBox->value();
                    if (stored.hourSlider->value() < loopStart ||
                        stored.hourSlider->value() > loopEnd)
                    {
                       stored.hourSlider->setValue(loopStart);
                    }

                    // Warms the disk cache for every forecast hour this
                    // loop will visit, in the background, so per-step
                    // playback doesn't block on a fresh ~320MB fetch (see
                    // GribManager::PrefetchRrfsForecastHourRange()'s own
                    // doc, and SetRrfsLoopRange()'s for why this is
                    // usually much less than the full cycle).
                    stored.gribManager->PrefetchRrfsForecastHourRange();
                    stored.animationTimer->start();
                    stored.playButton->setText(tr("Pause"));
                 }
              });

      connect(stored.animationTimer,
              &QTimer::timeout,
              self_,
              [&stored]()
              {
                 // Loops back to the loop range's own start rather than
                 // stopping at its end -- "keep watching the run evolve"
                 // is the whole point of Play, so wrapping (not halting)
                 // matches that intent, same as the original always-
                 // [0, max] behavior this generalizes.
                 int next = stored.hourSlider->value() + 1;
                 if (next > stored.loopEndSpinBox->value())
                 {
                    next = stored.loopStartSpinBox->value();
                 }
                 stored.hourSlider->setValue(next);
              });
   }

   RefreshSection(stored);
}

void GribDockWidget::Impl::RefreshSection(CategorySection& section)
{
   const auto activeNames = section.gribManager->ActiveProductNames();

   if (activeNames.empty())
   {
      // Never actually reachable today (GribManager always keeps at
      // least one product active -- see its own SetProductActive), kept
      // as a defensive display case rather than assuming that invariant
      // holds forever.
      section.statusLabel->setText(tr("(no products active)"));
      return;
   }

   // The primary (CurrentProductIndex()) product's valid time --
   // GribProductLayer only ever renders that one today, so its status is
   // what's actually meaningful to show here; the other active products
   // are fetching/decoding, just not drawn on the map yet.
   const std::string validTime =
      map::ReadGribFrameValidTime(map::GetGribFramePath(
         section.category, section.gribManager->CurrentProductIndex()));

   std::string statusText =
      "Primary: " + section.gribManager->CurrentProductName();
   statusText +=
      validTime.empty() ? "\n(no frame loaded)" : "\nValid: " + validTime;

   if (activeNames.size() > 1)
   {
      statusText += fmt::format("\n({} products active)", activeNames.size());
   }

   section.statusLabel->setText(QString::fromStdString(statusText));
}

GribDockWidget::GribDockWidget(QWidget* parent) :
    QDockWidget(parent), p {std::make_unique<Impl>(this)}
{
   setObjectName("GribDockWidget");
   setWindowTitle(tr("GRIB"));

   auto* contents = new QWidget(this);
   auto* layout   = new QVBoxLayout(contents);

   // Fixed at 3 (Mrms/Rtma/Rrfs) -- reserved upfront so BuildSection's
   // own push_back never reallocates mid-construction (see its comment).
   p->sections_.reserve(3);

   p->BuildSection(map::GribCategory::Mrms, layout, contents);
   p->BuildSection(map::GribCategory::Rtma, layout, contents);
   p->BuildSection(map::GribCategory::Rrfs, layout, contents);

   layout->addStretch();
   setWidget(contents);
}

GribDockWidget::~GribDockWidget() = default;

} // namespace scwx::qt::ui
