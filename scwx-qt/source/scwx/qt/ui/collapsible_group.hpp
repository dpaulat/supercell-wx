#pragma once

#include <QFrame>

namespace Ui
{
class CollapsibleGroup;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class CollapsibleGroupImpl;

class CollapsibleGroup : public QFrame
{
   Q_OBJECT

public:
   explicit CollapsibleGroup(QWidget* parent = nullptr);
   explicit CollapsibleGroup(const QString& title, QWidget* parent = nullptr);
   ~CollapsibleGroup();

   QLayout* GetContentsLayout();
   void     SetContentsLayout(QLayout* contents);
   void     SetTitle(const QString& title);

public slots:
   void SetExpanded(bool expanded);

signals:
   void StateChanged(bool expanded);

private:
   friend class CollapsibleGroupImpl;
   std::unique_ptr<CollapsibleGroupImpl> p;
   Ui::CollapsibleGroup*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
