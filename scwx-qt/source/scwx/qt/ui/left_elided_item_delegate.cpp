#include <scwx/qt/ui/left_elided_item_delegate.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

LeftElidedItemDelegate::LeftElidedItemDelegate(QObject* parent) :
    QStyledItemDelegate(parent)
{
}

LeftElidedItemDelegate::~LeftElidedItemDelegate() {}

void LeftElidedItemDelegate::paint(QPainter*                   painter,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex&          index) const
{
   QStyleOptionViewItem newOption = option;
   newOption.textElideMode        = Qt::TextElideMode::ElideLeft;
   QStyledItemDelegate::paint(painter, newOption, index);
}

} // namespace ui
} // namespace qt
} // namespace scwx
