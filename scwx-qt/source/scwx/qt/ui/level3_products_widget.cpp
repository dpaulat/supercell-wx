#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/util/logger.hpp>

#include <execution>
#include <shared_mutex>

#include <QMenu>
#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::level3_products_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class Level3ProductsWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit Level3ProductsWidgetImpl(Level3ProductsWidget* self) :
       self_ {self},
       layout_ {new ui::FlowLayout(self)},
       categoryButtons_ {},
       availableCategoryMap_ {},
       availableCategoryMutex_ {}
   {
      layout_->setContentsMargins(0, 0, 0, 0);

      for (common::Level3ProductCategory category :
           common::Level3ProductCategoryIterator())
      {
         QToolButton* toolButton = new QToolButton();
         toolButton->setText(
            QString::fromStdString(common::GetLevel3CategoryName(category)));
         toolButton->setStatusTip(
            tr(common::GetLevel3CategoryDescription(category).c_str()));
         toolButton->setPopupMode(QToolButton::MenuButtonPopup);
         layout_->addWidget(toolButton);
         categoryButtons_.push_back(toolButton);

         QObject::connect(toolButton,
                          &QToolButton::clicked,
                          this,
                          [=]() { SelectProductCategory(category); });

         QMenu* categoryMenu = new QMenu();
         toolButton->setMenu(categoryMenu);

         const auto& products = common::GetLevel3ProductsByCategory(category);
         auto&       productMenus = categoryMenuMap_[category];

         for (const auto& product : products)
         {
            QMenu* productMenu = categoryMenu->addMenu(QString::fromStdString(
               common::GetLevel3ProductDescription(product)));

            for (size_t tilt = 1; tilt <= common::kLevel3ProductMaxTilts;
                 ++tilt)
            {
               productMenu->addAction(tr("Tilt %1").arg(tilt));
            }

            productMenus[product] = productMenu;

            productMenu->menuAction()->setVisible(false);
         }

         toolButton->setEnabled(false);
      }
   }
   ~Level3ProductsWidgetImpl() = default;

   void NormalizeProductButtons();
   void SelectProductCategory(common::Level3ProductCategory category);
   void UpdateProductSelection(common::Level3ProductCategory category);

   Level3ProductsWidget*   self_;
   QLayout*                layout_;
   std::list<QToolButton*> categoryButtons_;
   std::unordered_map<common::Level3ProductCategory,
                      std::unordered_map<std::string, QMenu*>>
      categoryMenuMap_;

   common::Level3ProductCategoryMap availableCategoryMap_;
   std::shared_mutex                availableCategoryMutex_;
};

Level3ProductsWidget::Level3ProductsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Level3ProductsWidgetImpl>(this)}
{
}

Level3ProductsWidget::~Level3ProductsWidget() = default;

void Level3ProductsWidget::showEvent(QShowEvent* event)
{
   QWidget::showEvent(event);

   p->NormalizeProductButtons();
}

void Level3ProductsWidgetImpl::NormalizeProductButtons()
{
   int level3MaxWidth = 0;

   // Set each level 2 product's tool button to the same size
   std::for_each(categoryButtons_.cbegin(),
                 categoryButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->isVisible())
                    {
                       level3MaxWidth =
                          std::max(level3MaxWidth, toolButton->width());
                    }
                 });

   if (level3MaxWidth > 0)
   {
      std::for_each(categoryButtons_.cbegin(),
                    categoryButtons_.cend(),
                    [&](auto& toolButton)
                    { toolButton->setMinimumWidth(level3MaxWidth); });
   }
}

void Level3ProductsWidgetImpl::SelectProductCategory(
   common::Level3ProductCategory category)
{
   UpdateProductSelection(category);

   emit self_->RadarProductSelected(
      common::RadarProductGroup::Level3,
      common::GetLevel3CategoryDefaultProduct(category),
      0);
}

void Level3ProductsWidget::UpdateAvailableProducts(
   const common::Level3ProductCategoryMap& updatedCategoryMap)
{
   logger_->trace("UpdateAvailableProducts()");

   {
      std::unique_lock lock {p->availableCategoryMutex_};
      p->availableCategoryMap_ = updatedCategoryMap;
   }

   // Iterate through each category tool button
   std::for_each(
      // std::execution::par_unseq,
      p->categoryButtons_.cbegin(),
      p->categoryButtons_.cend(),
      [&](QToolButton* toolButton)
      {
         const std::string& categoryName = toolButton->text().toStdString();
         const common::Level3ProductCategory category =
            common::GetLevel3Category(categoryName);

         auto       availableProductMapIter = updatedCategoryMap.find(category);
         const bool categoryEnabled =
            (availableProductMapIter != updatedCategoryMap.cend());

         // Enable category if any products are available
         toolButton->setEnabled(categoryEnabled);

         if (categoryEnabled)
         {
            const auto& availableProductMap = availableProductMapIter->second;
            auto&       productMenus        = p->categoryMenuMap_.at(category);

            // Iterate through each product menu
            std::for_each(
               // std::execution::par_unseq,
               productMenus.cbegin(),
               productMenus.cend(),
               [&](const auto productMenu)
               {
                  auto availableAwipsIdIter =
                     availableProductMap.find(productMenu.first);
                  const bool productEnabled =
                     (availableAwipsIdIter != availableProductMap.cend());

                  // Enable product if it has AWIPS IDs available
                  productMenu.second->menuAction()->setVisible(productEnabled);

                  if (productEnabled)
                  {
                     // Determine number of tilts to display
                     size_t numTilts =
                        std::min(availableAwipsIdIter->second.size(),
                                 common::kLevel3ProductMaxTilts);
                     size_t currentTilt = 0;

                     QList<QAction*> tiltActionList =
                        productMenu.second->findChildren<QAction*>(
                           Qt::FindDirectChildrenOnly);

                     for (QAction* tiltAction : tiltActionList)
                     {
                        // Set the first n tilts to visible. The QAction list
                        // includes the productMenu's action, so one is
                        // added.
                        tiltAction->setVisible(++currentTilt <= numTilts + 1);
                     }
                  }
               });
         }
      });
}

void Level3ProductsWidget::UpdateProductSelection(
   common::RadarProductGroup group, const std::string& productName)
{
   if (group == common::RadarProductGroup::Level3)
   {
      common::Level3ProductCategory category =
         common::GetLevel3CategoryByProduct(productName);
      p->UpdateProductSelection(category);
   }
   else
   {
      p->UpdateProductSelection(common::Level3ProductCategory::Unknown);
   }
}

void Level3ProductsWidgetImpl::UpdateProductSelection(
   common::Level3ProductCategory category)
{
   const std::string& categoryName = common::GetLevel3CategoryName(category);

   std::for_each(std::execution::par_unseq,
                 categoryButtons_.cbegin(),
                 categoryButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->text().toStdString() == categoryName)
                    {
                       toolButton->setCheckable(true);
                       toolButton->setChecked(true);
                    }
                    else
                    {
                       toolButton->setChecked(false);
                       toolButton->setCheckable(false);
                    }
                 });
}

} // namespace ui
} // namespace qt
} // namespace scwx

#include "level3_products_widget.moc"
