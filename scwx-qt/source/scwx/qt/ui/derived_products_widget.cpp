#include <scwx/deriver/deriver_factory.hpp>
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

      for (const auto& category :
           deriver::DeriverFactory::GetDerivedProductCategories())
      {
         const std::string& categoryName = category;

         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) Qt owns the memory
         auto* toolButton = new QToolButton();
         toolButton->setText(categoryName.c_str());
         toolButton->setPopupMode(QToolButton::MenuButtonPopup);
         productsLayout_->addWidget(toolButton);
         QObject::connect(toolButton,
                          &QToolButton::clicked,
                          this,
                          [this, category]()
                          { SelectProductCategory(category); });
         // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) Qt owns the memory
         auto* categoryMenu = new QMenu(toolButton);
         toolButton->setMenu(categoryMenu);
         const auto& products =
            deriver::DeriverFactory::GetDerivedProductsInCategory(categoryName);
         auto& productMenus = categoryMenuMap_[categoryName];

         for (const auto& product : products)
         {
            const std::string& productName = product;
            QMenu* productMenu = categoryMenu->addMenu(productName.c_str());

            const auto& tilts =
               deriver::DeriverFactory::GetDerivedTiltsForProducts(productName);
            for (const auto& tilt : tilts)
            {
               const std::string& tiltName = tilt;
               QAction* action = productMenu->addAction(tiltName.c_str());

               QObject::connect(
                  action,
                  &QAction::triggered,
                  this,
                  [this, tiltName]()
                  {
                     Q_EMIT self_->RadarProductSelected(
                        common::RadarProductGroup::Derived, tiltName, 0);
                  });
            }

            productMenus[product] = productMenu;
         }
      }
   }

   ~DerivedProductsWidgetImpl() override                       = default;
   DerivedProductsWidgetImpl(const DerivedProductsWidgetImpl&) = delete;
   DerivedProductsWidgetImpl(DerivedProductsWidgetImpl&&)      = delete;
   DerivedProductsWidgetImpl&
   operator=(const DerivedProductsWidgetImpl&)                       = delete;
   DerivedProductsWidgetImpl& operator=(DerivedProductsWidgetImpl&&) = delete;

   void NormalizeProductButtons();
   void SelectProductCategory(const std::string& category);

   DerivedProductsWidget*  self_;
   QLayout*                layout_;
   QWidget*                productsWidget_;
   QLayout*                productsLayout_;
   std::list<QToolButton*> categoryButtons_;
   // TODO there is a lot more to go here

   std::unordered_map<std::string, std::unordered_map<std::string, QMenu*>>
      categoryMenuMap_;
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
   for (auto& toolButton : categoryButtons_)
   {
      if (toolButton->isVisible())
      {
         maxWidth = std::max(maxWidth, toolButton->width());
      }
   }

   if (maxWidth > 0)
   {
      for (auto& toolButton : categoryButtons_)
      {
         toolButton->setMinimumWidth(maxWidth);
      }
   }
}

void DerivedProductsWidgetImpl::SelectProductCategory(const std::string&)
{
   // UpdateCategorySelection(category);

   Q_EMIT self_->RadarProductSelected(
      common::RadarProductGroup::Derived, "SRV-AVG-0", 0);
}

} // namespace scwx::qt::ui

#include "derived_products_widget.moc"
