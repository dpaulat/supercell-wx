#pragma once

#include <QStyledItemDelegate>

namespace scwx
{
namespace qt
{
namespace ui
{

class LeftElidedItemDelegate : public QStyledItemDelegate
{
public:
   explicit LeftElidedItemDelegate(QObject* parent = nullptr);
   ~LeftElidedItemDelegate();

   void paint(QPainter*                   painter,
              const QStyleOptionViewItem& option,
              const QModelIndex&          index) const override;
};

} // namespace ui
} // namespace qt
} // namespace scwx
