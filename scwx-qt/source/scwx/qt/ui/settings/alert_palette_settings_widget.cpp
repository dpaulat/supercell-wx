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

   void     AddPhenomenonLine(const std::string&      name,
                              settings::LineSettings& lineSettings,
                              QGridLayout*            layout,
                              int                     row);
   QWidget* CreateStackedWidgetPage(awips::Phenomenon phenomenon);
   void     ConnectSignals();
   void     SelectPhenomenon(awips::Phenomenon phenomenon);
   void     SetupUi();

   AlertPaletteSettingsWidget* self_;

   QStackedWidget* phenomenonPagesWidget_;
   QListWidget*    phenomenonListView_;

   EditLineDialog* editLineDialog_;
   LineLabel*      activeLineLabel_ {nullptr};

   boost::unordered_flat_map<awips::Phenomenon, QWidget*> phenomenonPages_ {};
};

AlertPaletteSettingsWidget::AlertPaletteSettingsWidget(QWidget* parent) :
    SettingsPageWidget(parent), p {std::make_shared<Impl>(this)}
{
}

AlertPaletteSettingsWidget::~AlertPaletteSettingsWidget() = default;

void AlertPaletteSettingsWidget::Impl::SetupUi()
{
   // Setup phenomenon index pane
   QLabel* phenomenonLabel = new QLabel(tr("Phenomenon:"), self_);
   phenomenonPagesWidget_->setSizePolicy(QSizePolicy::Policy::MinimumExpanding,
                                         QSizePolicy::Policy::Preferred);

   // Setup stacked widget
   for (auto& phenomenon : settings::PaletteSettings::alert_phenomena())
   {
      QWidget* phenomenonWidget = CreateStackedWidgetPage(phenomenon);
      phenomenonPagesWidget_->addWidget(phenomenonWidget);

      phenomenonPages_.insert_or_assign(phenomenon, phenomenonWidget);

      phenomenonListView_->addItem(
         QString::fromStdString(awips::GetPhenomenonText(phenomenon)));
   }

   phenomenonListView_->setCurrentRow(0);

   // Create phenomenon index pane layout
   QVBoxLayout* phenomenonIndexLayout = new QVBoxLayout(self_);
   phenomenonIndexLayout->addWidget(phenomenonLabel);
   phenomenonIndexLayout->addWidget(phenomenonListView_);

   QWidget* phenomenonIndexPane = new QWidget(self_);
   phenomenonIndexPane->setLayout(phenomenonIndexLayout);

   // Create primary widget layout
   QGridLayout* gridLayout = new QGridLayout(self_);
   gridLayout->setContentsMargins(0, 0, 0, 0);
   gridLayout->addWidget(phenomenonIndexPane, 0, 0);
   gridLayout->addWidget(phenomenonPagesWidget_, 0, 1);

   QSpacerItem* spacer =
      new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
   gridLayout->addItem(spacer, 1, 0);

   self_->setLayout(gridLayout);
}

void AlertPaletteSettingsWidget::Impl::ConnectSignals()
{
   connect(
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

   connect(
      editLineDialog_,
      &EditLineDialog::accepted,
      self_,
      [this]()
      {
         // If the active line label was set
         if (activeLineLabel_ != nullptr)
         {
            // Update the active line label with selected line settings
            activeLineLabel_->set_border_color(editLineDialog_->border_color());
            activeLineLabel_->set_highlight_color(
               editLineDialog_->highlight_color());
            activeLineLabel_->set_line_color(editLineDialog_->line_color());

            activeLineLabel_->set_border_width(editLineDialog_->border_width());
            activeLineLabel_->set_highlight_width(
               editLineDialog_->highlight_width());
            activeLineLabel_->set_line_width(editLineDialog_->line_width());

            // Reset the active line label
            activeLineLabel_ = nullptr;
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

   const auto& impactBasedWarningInfo =
      awips::ibw::GetImpactBasedWarningInfo(phenomenon);

   auto& alertPalette =
      settings::PaletteSettings::Instance().alert_palette(phenomenon);

   int row = 0;

   // Add a blank label to align left and right widgets
   gridLayout->addWidget(new QLabel(self_), row++, 0);

   AddPhenomenonLine(
      "Active",
      alertPalette.threat_category(awips::ibw::ThreatCategory::Base),
      gridLayout,
      row++);

   if (impactBasedWarningInfo.hasObservedTag_)
   {
      AddPhenomenonLine("Observed", alertPalette.observed(), gridLayout, row++);
   }

   if (impactBasedWarningInfo.hasTornadoPossibleTag_)
   {
      AddPhenomenonLine("Tornado Possible",
                        alertPalette.tornado_possible(),
                        gridLayout,
                        row++);
   }

   for (auto& category : impactBasedWarningInfo.threatCategories_)
   {
      if (category == awips::ibw::ThreatCategory::Base)
      {
         continue;
      }

      AddPhenomenonLine(awips::ibw::GetThreatCategoryName(category),
                        alertPalette.threat_category(category),
                        gridLayout,
                        row++);
   }

   AddPhenomenonLine("Inactive", alertPalette.inactive(), gridLayout, row++);

   QSpacerItem* spacer =
      new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
   gridLayout->addItem(spacer, row, 0);

   return page;
}

void AlertPaletteSettingsWidget::Impl::AddPhenomenonLine(
   const std::string&      name,
   settings::LineSettings& lineSettings,
   QGridLayout*            layout,
   int                     row)
{
   QToolButton* toolButton = new QToolButton(self_);
   toolButton->setText(tr("..."));

   LineLabel* lineLabel = new LineLabel(self_);
   lineLabel->set_line_settings(lineSettings);

   layout->addWidget(new QLabel(tr(name.c_str()), self_), row, 0);
   layout->addWidget(lineLabel, row, 1);
   layout->addWidget(toolButton, row, 2);

   self_->AddSettingsCategory(&lineSettings);

   connect(
      toolButton,
      &QAbstractButton::clicked,
      self_,
      [this, lineLabel]()
      {
         // Set the active line label for when the dialog is finished
         activeLineLabel_ = lineLabel;

         // Initialize dialog with current line settings
         editLineDialog_->set_border_color(lineLabel->border_color());
         editLineDialog_->set_highlight_color(lineLabel->highlight_color());
         editLineDialog_->set_line_color(lineLabel->line_color());

         editLineDialog_->set_border_width(lineLabel->border_width());
         editLineDialog_->set_highlight_width(lineLabel->highlight_width());
         editLineDialog_->set_line_width(lineLabel->line_width());

         // Show the dialog
         editLineDialog_->show();
      });
}

} // namespace ui
} // namespace qt
} // namespace scwx
