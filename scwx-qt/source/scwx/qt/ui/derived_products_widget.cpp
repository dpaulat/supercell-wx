#include <scwx/qt/ui/derived_products_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/util/logger.hpp>

// #include <shared_mutex>
// #include <unordered_map>

#include <QCheckBox>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

namespace scwx::qt::ui
{

class DerivedProductsWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit DerivedProductsWidgetImpl(DerivedProductsWidget* self) :
       self_ {self},
       layout_ {new QVBoxLayout(self)},
       productsWidget_ {new QWidget(self)},
       productsLayout_ {new ui::FlowLayout(productsWidget_)},
       categoryButtons_ {}
   {
      layout_->setContentsMargins(0, 0, 0, 0);
      layout_->addWidget(productsWidget_);
      productsLayout_->setContentsMargins(0, 0, 0, 0);

      // TODO
      QToolButton* toolButton = new QToolButton();
      toolButton->setText("SRV");
      toolButton->setStatusTip("Text SRV product. One elevation only.");
      productsLayout_->addWidget(toolButton);
      categoryButtons_.push_back(toolButton);
      QObject::connect(toolButton,
                       &QToolButton::clicked,
                       this,
                       [this]() { SelectProductCategory("SRV"); });
   }

   ~DerivedProductsWidgetImpl() override = default;
   DerivedProductsWidgetImpl(const DerivedProductsWidgetImpl&) = delete;
   DerivedProductsWidgetImpl(DerivedProductsWidgetImpl&&)      = delete;
   DerivedProductsWidgetImpl&
   operator=(const DerivedProductsWidgetImpl&)                       = delete;
   DerivedProductsWidgetImpl& operator=(DerivedProductsWidgetImpl&&) = delete;

   void NormalizeProductButtons();
   void SelectProductCategory(std::string category);

   DerivedProductsWidget*  self_;
   QLayout*                layout_;
   QWidget*                productsWidget_;
   QLayout*                productsLayout_;
   std::list<QToolButton*> categoryButtons_;
   // TODO there is a lot more to go here

};

DerivedProductsWidget::DerivedProductsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<DerivedProductsWidgetImpl>(this)}
{
}

DerivedProductsWidget::~DerivedProductsWidget() = default;

void DerivedProductsWidget::showEvent(QShowEvent* event)
{
   QWidget::showEvent(event);

   p->NormalizeProductButtons();
}

void DerivedProductsWidgetImpl::NormalizeProductButtons()
{
   int maxWidth = 0;

   // Set each level 2 product's tool button to the same size
   std::for_each(categoryButtons_.cbegin(),
                 categoryButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->isVisible())
                    {
                       maxWidth =
                          std::max(maxWidth, toolButton->width());
                    }
                 });

   if (maxWidth > 0)
   {
      std::for_each(categoryButtons_.cbegin(),
                    categoryButtons_.cend(),
                    [&](auto& toolButton)
                    { toolButton->setMinimumWidth(maxWidth); });
   }
}

void DerivedProductsWidgetImpl::SelectProductCategory(std::string category)
{
   //UpdateCategorySelection(category);

   Q_EMIT self_->RadarProductSelected(
      common::RadarProductGroup::Derived,
      category,
      0);
}


} // namespace scwx::qt::ui

#include "derived_products_widget.moc"
