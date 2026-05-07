#include "collapsible_group.hpp"
#include "ui_collapsible_group.h"

#include <QEvent>
#include <QIcon>
#include <QPalette>
#include <QString>
#include <QStyle>

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
   void RefreshTitleButton();
   void UpdateIcon();

   const QString kTitleButtonStyleSheet_ {"text-align: left;"};
   const QString kCollapsedIcon_ {
      ":/res/icons/font-awesome-6/square-caret-right-regular.svg"};
   const QString kExpandedIcon_ {
      ":/res/icons/font-awesome-6/square-caret-down-regular.svg"};

   const std::map<bool, const QString&> kIcon_ {{false, kCollapsedIcon_},
                                                {true, kExpandedIcon_}};

   CollapsibleGroup* self_;

   bool expanded_ {true};
   bool refreshingTitleButton_ {false};
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

void CollapsibleGroupImpl::UpdateIcon()
{
   self_->ui->titleButton->setIcon(QIcon {kIcon_.at(expanded_)});
}

void CollapsibleGroupImpl::RefreshTitleButton()
{
   if (refreshingTitleButton_)
   {
      return;
   }

   refreshingTitleButton_ = true;
   auto* titleButton      = self_->ui->titleButton;

   // Qt's stylesheet engine can keep palette-derived button colors after
   // live theme changes, especially when the dock is floated/reparented.
   titleButton->setPalette(QPalette {});
   titleButton->setAttribute(Qt::WA_SetPalette, false);
   titleButton->setStyleSheet(QString {});
   titleButton->setStyleSheet(kTitleButtonStyleSheet_);

   if (titleButton->style() != nullptr)
   {
      titleButton->style()->unpolish(titleButton);
      titleButton->style()->polish(titleButton);
   }

   UpdateIcon();
   titleButton->update();
   refreshingTitleButton_ = false;
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
   const bool stateChanged = p->expanded_ != expanded;
   p->expanded_            = expanded;
   p->RefreshTitleButton();

   // Update contents visibility
   ui->contentsFrame->setVisible(expanded);

   // Update internal state
   if (stateChanged)
   {
      Q_EMIT StateChanged(expanded);
   }
}

void CollapsibleGroup::changeEvent(QEvent* event)
{
   QFrame::changeEvent(event);

   switch (event->type())
   {
   case QEvent::PaletteChange:
   case QEvent::ApplicationPaletteChange:
   case QEvent::StyleChange:
   case QEvent::ParentChange:
      // Reparent (e.g. radar toolbox dock floated) does not always emit
      // palette/style changes; refresh button and caret for current theme.
      p->RefreshTitleButton();
      break;
   default:
      break;
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
