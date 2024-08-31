#include <scwx/qt/ui/settings/alert_palette_settings_widget.hpp>
#include <scwx/qt/ui/edit_line_dialog.hpp>
#include <scwx/qt/ui/line_label.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/awips/phenomenon.hpp>

#include <boost/unordered/unordered_flat_map.hpp>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ =
   "scwx::qt::ui::settings::alert_palette_settings_widget";

struct PhenomenonInfo
{
   bool                               hasObservedTag_ {false};
   bool                               hasTornadoPossibleTag_ {false};
   std::vector<awips::ThreatCategory> threatCategories_ {
      awips::ThreatCategory::Base};
};

static const boost::unordered_flat_map<awips::Phenomenon, PhenomenonInfo>
   phenomenaInfo_ {{awips::Phenomenon::Marine,
                    PhenomenonInfo {.hasTornadoPossibleTag_ {true}}},
                   {awips::Phenomenon::FlashFlood,
                    PhenomenonInfo {.threatCategories_ {
                       awips::ThreatCategory::Base,
                       awips::ThreatCategory::Considerable,
                       awips::ThreatCategory::Catastrophic}}},
                   {awips::Phenomenon::SevereThunderstorm,
                    PhenomenonInfo {.hasTornadoPossibleTag_ {true},
                                    .threatCategories_ {
                                       awips::ThreatCategory::Base,
                                       awips::ThreatCategory::Considerable,
                                       awips::ThreatCategory::Destructive}}},
                   {awips::Phenomenon::SnowSquall, PhenomenonInfo {}},
                   {awips::Phenomenon::Tornado,
                    PhenomenonInfo {.hasObservedTag_ {true},
                                    .threatCategories_ {
                                       awips::ThreatCategory::Base,
                                       awips::ThreatCategory::Considerable,
                                       awips::ThreatCategory::Catastrophic}}}};

class AlertPaletteSettingsWidget::Impl
{
public:
   explicit Impl(AlertPaletteSettingsWidget* self) :
       self_ {self},
       phenomenonPagesWidget_ {new QStackedWidget(self)},
       phenomenonListView_ {new QListWidget(self)},
       editLineDialog_ {new EditLineDialog(self)}
   {
      SetupUi();
      ConnectSignals();
   }
   ~Impl() = default;

   void
   AddPhenomenonLine(const std::string& name, QGridLayout* layout, int row);
   QWidget* CreateStackedWidgetPage(awips::Phenomenon phenomenon);
   void     ConnectSignals();
   void     SelectPhenomenon(awips::Phenomenon phenomenon);
   void     SetupUi();

   AlertPaletteSettingsWidget* self_;

   QStackedWidget* phenomenonPagesWidget_;
   QListWidget*    phenomenonListView_;

   EditLineDialog* editLineDialog_;

   boost::unordered_flat_map<awips::Phenomenon, QWidget*> phenomenonPages_ {};
};

AlertPaletteSettingsWidget::AlertPaletteSettingsWidget(QWidget* parent) :
    SettingsPageWidget(parent), p {std::make_shared<Impl>(this)}
{
}

AlertPaletteSettingsWidget::~AlertPaletteSettingsWidget() = default;

void AlertPaletteSettingsWidget::Impl::SetupUi()
{
   // Setup primary widget layout
   QGridLayout* gridLayout = new QGridLayout(self_);
   gridLayout->setContentsMargins(0, 0, 0, 0);
   self_->setLayout(gridLayout);

   QWidget* phenomenonIndexPane = new QWidget(self_);
   phenomenonPagesWidget_->setSizePolicy(QSizePolicy::Policy::Expanding,
                                         QSizePolicy::Policy::Preferred);

   gridLayout->addWidget(phenomenonIndexPane, 0, 0);
   gridLayout->addWidget(phenomenonPagesWidget_, 0, 1);

   QSpacerItem* spacer =
      new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
   gridLayout->addItem(spacer, 1, 0);

   // Setup phenomenon index pane
   QVBoxLayout* phenomenonIndexLayout = new QVBoxLayout(self_);
   phenomenonIndexPane->setLayout(phenomenonIndexLayout);

   QLabel* phenomenonLabel = new QLabel(tr("Phenomenon:"), self_);
   phenomenonListView_->setSizePolicy(QSizePolicy::Policy::Minimum,
                                      QSizePolicy::Policy::Expanding);

   phenomenonIndexLayout->addWidget(phenomenonLabel);
   phenomenonIndexLayout->addWidget(phenomenonListView_);

   // Setup stacked widget
   auto& paletteSettings = settings::PaletteSettings::Instance();
   Q_UNUSED(paletteSettings);

   for (auto& phenomenon : settings::PaletteSettings::alert_phenomena())
   {
      QWidget* phenomenonWidget = CreateStackedWidgetPage(phenomenon);
      phenomenonPagesWidget_->addWidget(phenomenonWidget);

      phenomenonPages_.insert_or_assign(phenomenon, phenomenonWidget);

      phenomenonListView_->addItem(
         QString::fromStdString(awips::GetPhenomenonText(phenomenon)));
   }

   phenomenonListView_->setCurrentRow(0);
}

void AlertPaletteSettingsWidget::Impl::ConnectSignals()
{
   QObject::connect(
      phenomenonListView_->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      self_,
      [this](const QItemSelection& selected, const QItemSelection& deselected)
      {
         if (selected.size() == 0 && deselected.size() == 0)
         {
            // Items which stay selected but change their index are not
            // included in selected and deselected. Thus, this signal might
            // be emitted with both selected and deselected empty, if only
            // the indices of selected items change.
            return;
         }

         if (selected.size() > 0)
         {
            QModelIndex selectedIndex = selected[0].indexes()[0];
            QVariant    variantData =
               phenomenonListView_->model()->data(selectedIndex);
            if (variantData.typeId() == QMetaType::QString)
            {
               awips::Phenomenon phenomenon = awips::GetPhenomenonFromText(
                  variantData.toString().toStdString());
               SelectPhenomenon(phenomenon);
            }
         }
      });
}

void AlertPaletteSettingsWidget::Impl::SelectPhenomenon(
   awips::Phenomenon phenomenon)
{
   auto it = phenomenonPages_.find(phenomenon);
   if (it != phenomenonPages_.cend())
   {
      phenomenonPagesWidget_->setCurrentWidget(it->second);
   }
}

QWidget* AlertPaletteSettingsWidget::Impl::CreateStackedWidgetPage(
   awips::Phenomenon phenomenon)
{
   QWidget*     page       = new QWidget(self_);
   QGridLayout* gridLayout = new QGridLayout(self_);
   page->setLayout(gridLayout);

   const auto& phenomenonInfo = phenomenaInfo_.at(phenomenon);

   int row = 0;

   // Add a blank label to align left and right widgets
   gridLayout->addWidget(new QLabel(self_), row++, 0);

   AddPhenomenonLine("Active", gridLayout, row++);

   if (phenomenonInfo.hasObservedTag_)
   {
      AddPhenomenonLine("Observed", gridLayout, row++);
   }

   if (phenomenonInfo.hasTornadoPossibleTag_)
   {
      AddPhenomenonLine("Tornado Possible", gridLayout, row++);
   }

   for (auto& category : phenomenonInfo.threatCategories_)
   {
      if (category == awips::ThreatCategory::Base)
      {
         continue;
      }

      AddPhenomenonLine(
         awips::GetThreatCategoryName(category), gridLayout, row++);
   }

   AddPhenomenonLine("Inactive", gridLayout, row++);

   QSpacerItem* spacer =
      new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
   gridLayout->addItem(spacer, row, 0);

   return page;
}

void AlertPaletteSettingsWidget::Impl::AddPhenomenonLine(
   const std::string& name, QGridLayout* layout, int row)
{
   layout->addWidget(new QLabel(tr(name.c_str()), self_), row, 0);
   layout->addWidget(new LineLabel(self_), row, 1);
   layout->addWidget(new QToolButton(self_), row, 2);
}

} // namespace ui
} // namespace qt
} // namespace scwx
