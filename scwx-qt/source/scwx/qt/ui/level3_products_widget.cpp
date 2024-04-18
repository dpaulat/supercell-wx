#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/util/logger.hpp>

#include <execution>
#include <shared_mutex>

#include <QCheckBox>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

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
       layout_ {new QVBoxLayout(self)},
       productsWidget_ {new QWidget(self)},
       productsLayout_ {new ui::FlowLayout(productsWidget_)},
       categoryButtons_ {},
       productTiltMap_ {},
       awipsProductMap_ {},
       awipsProductMutex_ {}
   {
      layout_->setContentsMargins(0, 0, 0, 0);
      layout_->addWidget(productsWidget_);
      productsLayout_->setContentsMargins(0, 0, 0, 0);

      for (common::Level3ProductCategory category :
           common::Level3ProductCategoryIterator())
      {
         QToolButton* toolButton = new QToolButton();
         toolButton->setText(
            QString::fromStdString(common::GetLevel3CategoryName(category)));
         toolButton->setStatusTip(
            tr(common::GetLevel3CategoryDescription(category).c_str()));
         toolButton->setPopupMode(QToolButton::MenuButtonPopup);
         productsLayout_->addWidget(toolButton);
         categoryButtons_.push_back(toolButton);

         QObject::connect(toolButton,
                          &QToolButton::clicked,
                          this,
                          [=, this]() { SelectProductCategory(category); });

         QMenu* categoryMenu = new QMenu();
         toolButton->setMenu(categoryMenu);

         const auto& products = common::GetLevel3ProductsByCategory(category);
         auto&       productMenus = categoryMenuMap_[category];

         for (const auto& product : products)
         {
            QMenu* productMenu = categoryMenu->addMenu(QString::fromStdString(
               common::GetLevel3ProductDescription(product)));

            std::vector<QAction*>& productTilts = productTiltMap_[product];

            for (size_t tilt = 1; tilt <= common::kLevel3ProductMaxTilts;
                 ++tilt)
            {
               QAction* action =
                  productMenu->addAction(tr("Tilt %1").arg(tilt));
               action->setCheckable(true);

               std::unique_lock lock {awipsProductMutex_};

               productTilts.push_back(action);
               awipsProductMap_.emplace(action, "?");

               QObject::connect(
                  action,
                  &QAction::triggered,
                  this,
                  [=, this]()
                  {
                     std::shared_lock lock {awipsProductMutex_};
                     std::string awipsProductName {awipsProductMap_.at(action)};

                     self_->UpdateProductSelection(
                        common::RadarProductGroup::Level3, awipsProductName);

                     Q_EMIT self_->RadarProductSelected(
                        common::RadarProductGroup::Level3, awipsProductName, 0);
                  });
            }

            productMenus[product] = productMenu;

            productMenu->menuAction()->setVisible(false);
         }

         toolButton->setEnabled(false);
      }

      // Storm Tracking Information
      QCheckBox* stiPastEnableCheckBox     = new QCheckBox();
      QCheckBox* stiForecastEnableCheckBox = new QCheckBox();

      stiPastEnableCheckBox->setText(QObject::tr("Storm Tracks (Past)"));
      stiForecastEnableCheckBox->setText(
         QObject::tr("Storm Tracks (Forecast)"));

      layout_->addWidget(stiPastEnableCheckBox);
      layout_->addWidget(stiForecastEnableCheckBox);

      auto& productSettings = settings::ProductSettings::Instance();

      stiPastEnabled_.SetSettingsVariable(productSettings.sti_past_enabled());
      stiForecastEnabled_.SetSettingsVariable(
         productSettings.sti_forecast_enabled());

      stiPastEnabled_.SetEditWidget(stiPastEnableCheckBox);
      stiForecastEnabled_.SetEditWidget(stiForecastEnableCheckBox);

      QObject::connect(hotkeyManager_.get(),
                       &manager::HotkeyManager::HotkeyPressed,
                       this,
                       &Level3ProductsWidgetImpl::HandleHotkeyPressed);
   }
   ~Level3ProductsWidgetImpl() = default;

   void HandleHotkeyPressed(types::Hotkey hotkey, bool isAutoRepeat);
   void NormalizeProductButtons();
   void SelectProductCategory(common::Level3ProductCategory category);
   void UpdateCategorySelection(common::Level3ProductCategory category);
   void UpdateProductSelection(const std::string& awipsId);

   Level3ProductsWidget*   self_;
   QLayout*                layout_;
   QWidget*                productsWidget_;
   QLayout*                productsLayout_;
   std::list<QToolButton*> categoryButtons_;
   std::unordered_map<common::Level3ProductCategory,
                      std::unordered_map<std::string, QMenu*>>
      categoryMenuMap_;

   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};

   std::unordered_map<std::string, std::vector<QAction*>> productTiltMap_;

   std::unordered_map<QAction*, std::string> awipsProductMap_;
   std::shared_mutex                         awipsProductMutex_;

   std::string currentAwipsId_ {};
   QAction*    currentProductTiltAction_ {nullptr};

   settings::SettingsInterface<bool> stiPastEnabled_ {};
   settings::SettingsInterface<bool> stiForecastEnabled_ {};
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

void Level3ProductsWidgetImpl::HandleHotkeyPressed(types::Hotkey hotkey,
                                                   bool          isAutoRepeat)
{
   if (hotkey != types::Hotkey::ProductTiltDecrease &&
       hotkey != types::Hotkey::ProductTiltIncrease)
   {
      // Not handling this hotkey
      return;
   }

   logger_->trace("Handling hotkey: {}, repeat: {}",
                  types::GetHotkeyShortName(hotkey),
                  isAutoRepeat);

   std::string currentAwipsId           = currentAwipsId_;
   QAction*    currentProductTiltAction = currentProductTiltAction_;

   if (currentProductTiltAction == nullptr || currentAwipsId.empty() ||
       currentAwipsId == "?")
   {
      // Level 3 product is not selected
      return;
   }

   // Get product
   std::string product = common::GetLevel3ProductByAwipsId(currentAwipsId);
   if (product == "?")
   {
      logger_->error("Invalid AWIPS ID: {}", currentAwipsId);
      return;
   }

   std::shared_lock lock {awipsProductMutex_};

   // Find the current product tilt
   auto productTiltsIt = productTiltMap_.find(product);
   if (productTiltsIt == productTiltMap_.cend())
   {
      logger_->error("Could not find product tilt map: {}",
                     common::GetLevel3ProductDescription(product));
      return;
   }

   auto& productTilts  = productTiltsIt->second;
   auto  productTiltIt = std::find(
      productTilts.cbegin(), productTilts.cend(), currentProductTiltAction);
   if (productTiltIt == productTilts.cend())
   {
      logger_->error("Could not locate product tilt: {}", currentAwipsId);
      return;
   }

   std::ptrdiff_t productTiltIndex =
      std::distance(productTilts.cbegin(), productTiltIt);

   // Determine the new product tilt index
   std::ptrdiff_t newProductTiltIndex =
      (hotkey == types::Hotkey::ProductTiltDecrease) ? productTiltIndex - 1 :
                                                       productTiltIndex + 1;

   // Validate the new product tilt index
   if (newProductTiltIndex < 0 ||
       newProductTiltIndex >=
          static_cast<std::ptrdiff_t>(productTilts.size()) ||
       !productTilts.at(newProductTiltIndex)->isVisible())
   {
      const std::string direction =
         (hotkey == types::Hotkey::ProductTiltDecrease) ? "lower" : "upper";

      logger_->info("Product tilt at {} limit", direction);

      return;
   }

   // Select the new tilt
   productTilts.at(newProductTiltIndex)->trigger();
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
   UpdateCategorySelection(category);

   Q_EMIT self_->RadarProductSelected(
      common::RadarProductGroup::Level3,
      common::GetLevel3CategoryDefaultProduct(category),
      0);
}

void Level3ProductsWidget::UpdateAvailableProducts(
   const common::Level3ProductCategoryMap& updatedCategoryMap)
{
   logger_->trace("UpdateAvailableProducts()");

   // Iterate through each category tool button
   std::for_each(
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
                     const size_t numTilts =
                        std::min(availableAwipsIdIter->second.size(),
                                 common::kLevel3ProductMaxTilts);

                     const std::vector<QAction*>& tiltActionList =
                        p->productTiltMap_.at(productMenu.first);

                     for (size_t i = 0; i < tiltActionList.size(); i++)
                     {
                        bool visible = (i < numTilts);
                        tiltActionList[i]->setVisible(visible);

                        std::unique_lock lock {p->awipsProductMutex_};

                        p->awipsProductMap_[tiltActionList[i]] =
                           visible ? availableAwipsIdIter->second[i] : "?";
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
         common::GetLevel3CategoryByAwipsId(productName);
      p->UpdateCategorySelection(category);
      p->UpdateProductSelection(productName);
   }
   else
   {
      p->UpdateCategorySelection(common::Level3ProductCategory::Unknown);
      p->currentAwipsId_.erase();
      p->currentProductTiltAction_ = nullptr;
   }
}

void Level3ProductsWidgetImpl::UpdateCategorySelection(
   common::Level3ProductCategory category)
{
   const std::string& categoryName = common::GetLevel3CategoryName(category);

   std::for_each(categoryButtons_.cbegin(),
                 categoryButtons_.cend(),
                 [&, this](auto& toolButton)
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

void Level3ProductsWidgetImpl::UpdateProductSelection(
   const std::string& awipsId)
{
   std::shared_lock lock {awipsProductMutex_};

   QAction* newProductTilt = nullptr;

   std::for_each(awipsProductMap_.cbegin(),
                 awipsProductMap_.cend(),
                 [&, this](const auto& pair)
                 {
                    if (pair.second == awipsId)
                    {
                       newProductTilt = pair.first;
                       pair.first->setChecked(true);
                    }
                    else
                    {
                       pair.first->setChecked(false);
                    }
                 });

   currentAwipsId_           = awipsId;
   currentProductTiltAction_ = newProductTilt;
}

} // namespace ui
} // namespace qt
} // namespace scwx

#include "level3_products_widget.moc"
