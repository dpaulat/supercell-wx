#pragma once

#include <QStyledItemDelegate>

namespace scwx::qt::ui
{

// Editor for LayerModel::Column::DisplayedPanes: a compact "1-3,5,8"-style
// summary is shown normally (via the model's own DisplayRole, no custom
// painting needed here), and clicking it opens a small popup of the real
// per-pane checkboxes (DisplayMap1..9) for editing -- rather than showing
// up to 9 separate checkbox columns, which don't fit when the Layer
// Manager is docked narrow (e.g. in a sidebar).
//
// Each checkbox writes straight to its own DisplayMap column via
// QAbstractItemModel::setData() the moment it's toggled (immediate
// effect, matching how the individual checkbox columns already behave),
// rather than waiting for the popup to close -- so setEditorData()/
// setModelData() have nothing to do.
class LayerDisplayedPanesDelegate : public QStyledItemDelegate
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(LayerDisplayedPanesDelegate)

public:
   explicit LayerDisplayedPanesDelegate(QObject* parent = nullptr);
   ~LayerDisplayedPanesDelegate() override;

   QWidget* createEditor(QWidget*                    parent,
                         const QStyleOptionViewItem& option,
                         const QModelIndex&          index) const override;
   void setEditorData(QWidget* editor, const QModelIndex& index) const override;
   void setModelData(QWidget*            editor,
                     QAbstractItemModel* model,
                     const QModelIndex&  index) const override;
   void updateEditorGeometry(QWidget*                    editor,
                             const QStyleOptionViewItem& option,
                             const QModelIndex&          index) const override;
};

} // namespace scwx::qt::ui
