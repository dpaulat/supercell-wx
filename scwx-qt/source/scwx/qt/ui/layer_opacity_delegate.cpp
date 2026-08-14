#include <scwx/qt/ui/layer_opacity_delegate.hpp>
#include <scwx/qt/ui/widgets/focused_spin_box.hpp>

namespace scwx::qt::ui
{

LayerOpacityDelegate::LayerOpacityDelegate(QObject* parent) :
    QStyledItemDelegate(parent)
{
}
LayerOpacityDelegate::~LayerOpacityDelegate() = default;

QWidget*
LayerOpacityDelegate::createEditor(QWidget* parent,
                                   const QStyleOptionViewItem& /* option */,
                                   const QModelIndex& /* index */) const
{
   auto* spinBox = new QFocusedSpinBox(parent);
   spinBox->setRange(0, 100);
   spinBox->setSuffix(tr("%"));
   spinBox->setKeyboardTracking(false);
   return spinBox;
}

void LayerOpacityDelegate::setEditorData(QWidget*           editor,
                                         const QModelIndex& index) const
{
   auto* spinBox = qobject_cast<QFocusedSpinBox*>(editor);
   if (spinBox != nullptr)
   {
      spinBox->setValue(index.data(Qt::ItemDataRole::EditRole).toInt());
   }
}

void LayerOpacityDelegate::setModelData(QWidget*            editor,
                                        QAbstractItemModel* model,
                                        const QModelIndex&  index) const
{
   auto* spinBox = qobject_cast<QFocusedSpinBox*>(editor);
   if (spinBox != nullptr)
   {
      model->setData(index, spinBox->value(), Qt::ItemDataRole::EditRole);
   }
}

void LayerOpacityDelegate::updateEditorGeometry(
   QWidget*                    editor,
   const QStyleOptionViewItem& option,
   const QModelIndex& /* index */) const
{
   editor->setGeometry(option.rect);
}

} // namespace scwx::qt::ui
