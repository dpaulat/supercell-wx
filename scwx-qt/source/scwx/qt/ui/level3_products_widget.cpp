#include <scwx/qt/ui/level3_products_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/util/logger.hpp>

#include <execution>

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
       self_ {self}, layout_ {new ui::FlowLayout(self)}, categoryButtons_ {}
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
      }
   }
   ~Level3ProductsWidgetImpl() = default;

   void NormalizeProductButtons();
   void SelectProductCategory(common::Level3ProductCategory category);
   void UpdateProductSelection(common::Level3ProductCategory category);

   Level3ProductsWidget*   self_;
   QLayout*                layout_;
   std::list<QToolButton*> categoryButtons_;
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
