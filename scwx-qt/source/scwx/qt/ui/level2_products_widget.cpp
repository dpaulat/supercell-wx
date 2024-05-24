#include <scwx/qt/ui/level2_products_widget.hpp>
#include <scwx/qt/ui/flow_layout.hpp>
#include <scwx/qt/manager/hotkey_manager.hpp>
#include <scwx/util/logger.hpp>

#include <execution>

#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::level2_products_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const std::unordered_map<types::Hotkey, common::Level2Product>
   kHotkeyProductMap_ {
      {types::Hotkey::SelectLevel2Ref, common::Level2Product::Reflectivity},
      {types::Hotkey::SelectLevel2Vel, common::Level2Product::Velocity},
      {types::Hotkey::SelectLevel2SW, common::Level2Product::SpectrumWidth},
      {types::Hotkey::SelectLevel2ZDR,
       common::Level2Product::DifferentialReflectivity},
      {types::Hotkey::SelectLevel2Phi,
       common::Level2Product::DifferentialPhase},
      {types::Hotkey::SelectLevel2Rho,
       common::Level2Product::CorrelationCoefficient},
      {types::Hotkey::SelectLevel2CFP,
       common::Level2Product::ClutterFilterPowerRemoved}};

class Level2ProductsWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit Level2ProductsWidgetImpl(Level2ProductsWidget* self) :
       self_ {self}, layout_ {new ui::FlowLayout(self)}, productButtons_ {}
   {
      layout_->setContentsMargins(0, 0, 0, 0);

      for (common::Level2Product product : common::Level2ProductIterator())
      {
         QToolButton* toolButton = new QToolButton();
         toolButton->setText(
            QString::fromStdString(common::GetLevel2Name(product)));
         toolButton->setStatusTip(
            tr(common::GetLevel2Description(product).c_str()));
         layout_->addWidget(toolButton);
         productButtons_.push_back(toolButton);

         QObject::connect(toolButton,
                          &QToolButton::clicked,
                          this,
                          [=, this]() { SelectProduct(product); });

         QObject::connect(hotkeyManager_.get(),
                          &manager::HotkeyManager::HotkeyPressed,
                          this,
                          &Level2ProductsWidgetImpl::HandleHotkeyPressed);
      }
   }
   ~Level2ProductsWidgetImpl() = default;

   void HandleHotkeyPressed(types::Hotkey hotkey, bool isAutoRepeat);
   void NormalizeProductButtons();
   void SelectProduct(common::Level2Product product);
   void UpdateProductSelection(common::Level2Product product);

   Level2ProductsWidget*   self_;
   QLayout*                layout_;
   std::list<QToolButton*> productButtons_;

   std::shared_ptr<manager::HotkeyManager> hotkeyManager_ {
      manager::HotkeyManager::Instance()};
};

Level2ProductsWidget::Level2ProductsWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Level2ProductsWidgetImpl>(this)}
{
}

Level2ProductsWidget::~Level2ProductsWidget() = default;

void Level2ProductsWidget::showEvent(QShowEvent* event)
{
   QWidget::showEvent(event);

   p->NormalizeProductButtons();
}

void Level2ProductsWidgetImpl::HandleHotkeyPressed(types::Hotkey hotkey,
                                                   bool          isAutoRepeat)
{
   auto productIt = kHotkeyProductMap_.find(hotkey);

   if (productIt == kHotkeyProductMap_.cend())
   {
      // Not handling this hotkey
      return;
   }

   logger_->trace("Handling hotkey: {}, repeat: {}",
                  types::GetHotkeyShortName(hotkey),
                  isAutoRepeat);

   // Select product category hotkey
   SelectProduct(productIt->second);
}

void Level2ProductsWidgetImpl::NormalizeProductButtons()
{
   int level2MaxWidth = 0;

   // Set each level 2 product's tool button to the same size
   std::for_each(productButtons_.cbegin(),
                 productButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->isVisible())
                    {
                       level2MaxWidth =
                          std::max(level2MaxWidth, toolButton->width());
                    }
                 });

   if (level2MaxWidth > 0)
   {
      std::for_each(productButtons_.cbegin(),
                    productButtons_.cend(),
                    [&](auto& toolButton)
                    { toolButton->setMinimumWidth(level2MaxWidth); });
   }
}

void Level2ProductsWidgetImpl::SelectProduct(common::Level2Product product)
{
   UpdateProductSelection(product);

   Q_EMIT self_->RadarProductSelected(
      common::RadarProductGroup::Level2, common::GetLevel2Name(product), 0);
}

void Level2ProductsWidget::UpdateProductSelection(
   common::RadarProductGroup group, const std::string& productName)
{
   if (group == common::RadarProductGroup::Level2)
   {
      common::Level2Product product = common::GetLevel2Product(productName);
      p->UpdateProductSelection(product);
   }
   else
   {
      p->UpdateProductSelection(common::Level2Product::Unknown);
   }
}

void Level2ProductsWidgetImpl::UpdateProductSelection(
   common::Level2Product product)
{
   const std::string& productName = common::GetLevel2Name(product);

   std::for_each(productButtons_.cbegin(),
                 productButtons_.cend(),
                 [&](auto& toolButton)
                 {
                    if (toolButton->text().toStdString() == productName)
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

#include "level2_products_widget.moc"
