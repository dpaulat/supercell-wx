#include "placefile_settings_widget.hpp"
#include "ui_placefile_settings_widget.h"

#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/model/placefile_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/qt/ui/open_url_dialog.hpp>
#include <scwx/util/logger.hpp>

#include <QSortFilterProxyModel>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::placefile_settings_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

namespace
{

// Sits between PlacefileModel and the existing free-text-search proxy
// (chained, not merged, so the two filters -- category and the user's own
// search box -- both apply independently without a bespoke combined
// predicate). ShowAll (the default, matching every prior behavior) skips
// filtering entirely.
class CategoryFilterProxyModel : public QSortFilterProxyModel
{
public:
   explicit CategoryFilterProxyModel(PlacefileSettingsWidget::CategoryMode mode,
                                     std::string category,
                                     QObject*    parent = nullptr) :
       QSortFilterProxyModel(parent),
       mode_ {mode},
       category_ {std::move(category)}
   {
   }

protected:
   bool filterAcceptsRow(int                sourceRow,
                         const QModelIndex& sourceParent) const override
   {
      if (mode_ == PlacefileSettingsWidget::CategoryMode::ShowAll)
      {
         return true;
      }

      const QModelIndex index =
         sourceModel()->index(sourceRow, 0, sourceParent);
      const std::string rowCategory =
         sourceModel()
            ->data(index, types::ItemDataRole::CategoryRole)
            .toString()
            .toStdString();

      const bool matches = (rowCategory == category_);
      return mode_ == PlacefileSettingsWidget::CategoryMode::OnlyCategory ?
                matches :
                !matches;
   }

private:
   PlacefileSettingsWidget::CategoryMode mode_;
   std::string                           category_;
};

} // namespace

class PlacefileSettingsWidgetImpl
{
public:
   explicit PlacefileSettingsWidgetImpl(
      PlacefileSettingsWidget*              self,
      PlacefileSettingsWidget::CategoryMode categoryMode,
      const std::string&                    category) :
       self_ {self},
       openUrlDialog_ {new OpenUrlDialog(QObject::tr("Add Placefile"), self_)},
       placefileModel_ {new model::PlacefileModel(self_)},
       categoryFilterModel_ {
          new CategoryFilterProxyModel(categoryMode, category, self_)},
       placefileProxyModel_ {new QSortFilterProxyModel(self_)},
       categoryMode_ {categoryMode},
       category_ {category}
   {
      categoryFilterModel_->setSourceModel(placefileModel_);

      placefileProxyModel_->setSourceModel(categoryFilterModel_);
      placefileProxyModel_->setSortRole(types::ItemDataRole::SortRole);
      placefileProxyModel_->setFilterCaseSensitivity(
         Qt::CaseSensitivity::CaseInsensitive);
      placefileProxyModel_->setFilterKeyColumn(-1);
   }
   ~PlacefileSettingsWidgetImpl() = default;

   void ConnectSignals();

   PlacefileSettingsWidget* self_;
   OpenUrlDialog*           openUrlDialog_;

   std::shared_ptr<manager::PlacefileManager> placefileManager_ {
      manager::PlacefileManager::Instance()};

   model::PlacefileModel*    placefileModel_;
   CategoryFilterProxyModel* categoryFilterModel_;
   QSortFilterProxyModel*    placefileProxyModel_;

   // Used only so the Add button can tag a URL added from an OnlyCategory
   // view with that same category -- otherwise it would immediately
   // vanish from view (added, but filtered out of the tab the user just
   // used to add it).
   PlacefileSettingsWidget::CategoryMode categoryMode_;
   std::string                           category_;
};

PlacefileSettingsWidget::PlacefileSettingsWidget(QWidget*           parent,
                                                 CategoryMode       mode,
                                                 const std::string& category) :
    QFrame(parent),
    p {std::make_unique<PlacefileSettingsWidgetImpl>(this, mode, category)},
    ui(new Ui::PlacefileSettingsWidget)
{
   ui->setupUi(this);

   ui->removeButton->setEnabled(false);
   ui->refreshButton->setEnabled(false);

   ui->placefileView->setModel(p->placefileProxyModel_);

   auto placefileViewHeader = ui->placefileView->header();

   placefileViewHeader->setMinimumSectionSize(10);
   placefileViewHeader->setSortIndicator(
      static_cast<int>(model::PlacefileModel::Column::Placefile),
      Qt::AscendingOrder);

   // Enabled and Thresholds columns have a fixed size (checkbox)
   placefileViewHeader->setSectionResizeMode(
      static_cast<int>(model::PlacefileModel::Column::Enabled),
      QHeaderView::ResizeMode::ResizeToContents);
   placefileViewHeader->setSectionResizeMode(
      static_cast<int>(model::PlacefileModel::Column::Thresholds),
      QHeaderView::ResizeMode::ResizeToContents);

   p->ConnectSignals();
}

PlacefileSettingsWidget::~PlacefileSettingsWidget()
{ delete ui; }

void PlacefileSettingsWidgetImpl::ConnectSignals()
{
   QObject::connect(self_->ui->addButton,
                    &QPushButton::clicked,
                    self_,
                    [this]() { openUrlDialog_->open(); });

   QObject::connect(self_->ui->removeButton,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       auto selectionModel =
                          self_->ui->placefileView->selectionModel();

                       // Get selected URL string
                       QModelIndex selected =
                          selectionModel
                             ->selectedRows(static_cast<int>(
                                model::PlacefileModel::Column::Placefile))
                             .first();
                       QVariant data = self_->ui->placefileView->model()->data(
                          selected, types::ItemDataRole::SortRole);
                       std::string urlString = data.toString().toStdString();

                       // Remove Placefile
                       if (!urlString.empty())
                       {
                          placefileManager_->RemoveUrl(urlString);
                       }
                    });

   QObject::connect(self_->ui->refreshButton,
                    &QPushButton::clicked,
                    self_,
                    [this]()
                    {
                       auto selectionModel =
                          self_->ui->placefileView->selectionModel();

                       // Get selected URL string
                       QModelIndex selected =
                          selectionModel
                             ->selectedRows(static_cast<int>(
                                model::PlacefileModel::Column::Placefile))
                             .first();
                       QVariant data = self_->ui->placefileView->model()->data(
                          selected, types::ItemDataRole::SortRole);
                       std::string urlString = data.toString().toStdString();

                       // Refresh placefile
                       if (!urlString.empty())
                       {
                          placefileManager_->Refresh(urlString);
                       }
                    });

   QObject::connect(
      openUrlDialog_,
      &OpenUrlDialog::accepted,
      self_,
      [this]()
      {
         // Tag with this view's own category when it's showing only one
         // (the Outlooks tab) -- otherwise a URL added there would be
         // added, then immediately filtered right back out of view.
         const std::string category =
            categoryMode_ ==
                  PlacefileSettingsWidget::CategoryMode::OnlyCategory ?
               category_ :
               std::string {};
         placefileManager_->AddUrl(
            openUrlDialog_->url().toStdString(), {}, false, false, category);
      });

   QObject::connect(self_->ui->placefileFilter,
                    &QLineEdit::textChanged,
                    placefileProxyModel_,
                    &QSortFilterProxyModel::setFilterWildcard);

   QObject::connect(
      self_->ui->placefileView->selectionModel(),
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

         bool itemSelected = selected.size() > 0;
         self_->ui->removeButton->setEnabled(itemSelected);
         self_->ui->refreshButton->setEnabled(itemSelected);
      });
}

} // namespace ui
} // namespace qt
} // namespace scwx
