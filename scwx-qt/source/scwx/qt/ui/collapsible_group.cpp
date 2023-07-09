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
      ":/res/icons/font-awesome-6/square-caret-right-regular.svg"};
   const QIcon kExpandedIcon_ {
      ":/res/icons/font-awesome-6/square-caret-down-regular.svg"};

   const std::map<bool, const QIcon&> kIcon_ {{false, kCollapsedIcon_},
                                              {true, kExpandedIcon_}};

   CollapsibleGroup* self_;

   bool expanded_ {true};
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
   QObject::connect(
      self_->ui->titleButton,
      &QAbstractButton::clicked,
      self_,
      [this]() { self_->SetExpanded(!expanded_); },
      Qt::DirectConnection);

   self_->SetExpanded(true);
}

QLayout* CollapsibleGroup::GetContentsLayout()
{
   return ui->contentsFrame->layout();
}

void CollapsibleGroup::SetContentsLayout(QLayout* layout)
{
   ui->contentsFrame->setLayout(layout);
}

void CollapsibleGroup::SetTitle(const QString& title)
{
   ui->titleButton->setText(title);
}

void CollapsibleGroup::SetExpanded(bool expanded)
{
   // Update icon
   ui->titleButton->setIcon(p->kIcon_.at(expanded));

   // Update contents visibility
   ui->contentsFrame->setVisible(expanded);

   // Update internal state
   if (p->expanded_ != expanded)
   {
      p->expanded_ = expanded;

      Q_EMIT StateChanged(expanded);
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
