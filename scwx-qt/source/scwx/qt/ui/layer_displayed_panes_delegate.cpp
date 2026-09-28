#include <scwx/qt/ui/layer_displayed_panes_delegate.hpp>
#include <scwx/qt/model/layer_model.hpp>
#include <scwx/qt/settings/general_settings.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPersistentModelIndex>

namespace scwx::qt::ui
{

LayerDisplayedPanesDelegate::LayerDisplayedPanesDelegate(QObject* parent) :
    QStyledItemDelegate(parent)
{
}
LayerDisplayedPanesDelegate::~LayerDisplayedPanesDelegate() = default;

QWidget* LayerDisplayedPanesDelegate::createEditor(
   QWidget* parent,
   const QStyleOptionViewItem& /* option */,
   const QModelIndex& index) const
{
   // Qt::Popup: a real top-level window (not confined to the, possibly
   // narrow, cell it was opened from -- the whole reason this delegate
   // exists), and one that closes itself the moment the user clicks
   // outside it, same as a combo box dropdown.
   auto* popup  = new QWidget(parent, Qt::Popup);
   auto* layout = new QHBoxLayout(popup);
   layout->setContentsMargins(6, 6, 6, 6);
   layout->setSpacing(6);

   auto&     generalSettings = settings::GeneralSettings::Instance();
   const int mapCount =
      static_cast<int>(generalSettings.grid_width().GetValue() *
                       generalSettings.grid_height().GetValue());

   // Persistent, not a plain QModelIndex: this outlives createEditor()'s
   // own call, captured by each checkbox's toggled() lambda below.
   const QPersistentModelIndex rowIndex(index);

   for (int i = 0; i < mapCount; ++i)
   {
      const int paneColumn =
         static_cast<int>(model::LayerModel::Column::DisplayMap1) + i;

      auto* checkBox = new QCheckBox(QString::number(i + 1), popup);
      checkBox->setChecked(rowIndex.sibling(rowIndex.row(), paneColumn)
                              .data(Qt::ItemDataRole::CheckStateRole)
                              .toInt() ==
                           static_cast<int>(Qt::CheckState::Checked));

      // Writes back immediately on toggle, rather than waiting for the
      // popup to close -- lets the user flip several panes in one go and
      // see each take effect right away, same immediacy the individual
      // checkbox columns already had.
      QObject::connect(
         checkBox,
         &QCheckBox::toggled,
         popup,
         [rowIndex, paneColumn](bool checked)
         {
            // QModelIndex::model() is const-qualified even though the
            // model itself isn't -- the standard Qt workaround for a
            // delegate that needs to write back outside of
            // setModelData()'s own (non-const) model argument.
            auto* model = const_cast<QAbstractItemModel*>(rowIndex.model());
            if (model == nullptr)
            {
               return;
            }

            model->setData(rowIndex.sibling(rowIndex.row(), paneColumn),
                           checked ?
                              static_cast<int>(Qt::CheckState::Checked) :
                              static_cast<int>(Qt::CheckState::Unchecked),
                           Qt::ItemDataRole::CheckStateRole);
         });

      layout->addWidget(checkBox);
   }

   return popup;
}

void LayerDisplayedPanesDelegate::setEditorData(
   QWidget* /* editor */, const QModelIndex& /* index */) const
{
   // Each checkbox is already initialized from the model directly in
   // createEditor() -- nothing to do here.
}

void LayerDisplayedPanesDelegate::setModelData(
   QWidget* /* editor */,
   QAbstractItemModel* /* model */,
   const QModelIndex& /* index */) const
{
   // Each checkbox already wrote its own change back the moment it was
   // toggled (see createEditor()) -- there's no single value to commit
   // on close.
}

void LayerDisplayedPanesDelegate::updateEditorGeometry(
   QWidget*                    editor,
   const QStyleOptionViewItem& option,
   const QModelIndex& /* index */) const
{
   editor->adjustSize();

   const QPoint anchor =
      (option.widget != nullptr) ?
         option.widget->mapToGlobal(option.rect.bottomLeft()) :
         option.rect.bottomLeft();
   editor->move(anchor);
}

} // namespace scwx::qt::ui
