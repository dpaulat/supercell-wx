#include "collapsible_group.hpp"
#include "ui_collapsible_group.h"

namespace scwx
{
namespace qt
{
namespace ui
{

class CollapsibleGroupImpl
{
public:
   explicit CollapsibleGroupImpl(CollapsibleGroup* self) : self_ {self} {}
   ~CollapsibleGroupImpl() = default;

   void Initialize();

   const QIcon kCollapsedIcon_ {
      ":/res/icons/font-awesome-6/angle-right-solid.svg"};
   const QIcon kExpandedIcon_ {
      ":/res/icons/font-awesome-6/angle-down-solid.svg"};

   CollapsibleGroup* self_;
};

CollapsibleGroup::CollapsibleGroup(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<CollapsibleGroupImpl>(this)},
    ui(new Ui::CollapsibleGroup)
{
   ui->setupUi(this);
   p->Initialize();
}

CollapsibleGroup::CollapsibleGroup(const QString& title, QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<CollapsibleGroupImpl>(this)},
    ui(new Ui::CollapsibleGroup)
{
   ui->setupUi(this);
   ui->titleButton->setText(title);
   p->Initialize();
}

CollapsibleGroup::~CollapsibleGroup()
{
   delete ui;
}

void CollapsibleGroupImpl::Initialize()
{
   self_->Expand();

   QObject::connect(self_->ui->titleButton,
                    &QAbstractButton::toggled,
                    self_,
                    [this](bool checked)
                    {
                       if (checked)
                       {
                          self_->Expand();
                       }
                       else
                       {
                          self_->Collapse();
                       }
                    });
}

void CollapsibleGroup::SetContentsLayout(QLayout* layout)
{
   ui->contentsFrame->setLayout(layout);
}

void CollapsibleGroup::SetTitle(const QString& title)
{
   ui->titleButton->setText(title);
}

void CollapsibleGroup::Collapse()
{
   // Update the title frame
   if (ui->titleButton->isChecked())
   {
      ui->titleButton->setChecked(false);
   }

   // Hide the group contents
   ui->contentsFrame->setVisible(false);
}

void CollapsibleGroup::Expand()
{
   // Update the title frame
   if (!ui->titleButton->isChecked())
   {
      ui->titleButton->setChecked(true);
   }

   // Show the group contents
   ui->contentsFrame->setVisible(true);
}

} // namespace ui
} // namespace qt
} // namespace scwx
