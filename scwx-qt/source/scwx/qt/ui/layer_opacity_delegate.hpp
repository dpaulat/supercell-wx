#pragma once

#include <QStyledItemDelegate>

namespace scwx::qt::ui
{

class LayerOpacityDelegate : public QStyledItemDelegate
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(LayerOpacityDelegate)

public:
   explicit LayerOpacityDelegate(QObject* parent = nullptr);
   ~LayerOpacityDelegate() override;

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
