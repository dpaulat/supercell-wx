#pragma once

#include <scwx/common/products.hpp>

#include <QWidget>

namespace scwx::qt::ui
{

class DerivedProductsWidgetImpl;

class DerivedProductsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit DerivedProductsWidget(QWidget* parent = nullptr);
   ~DerivedProductsWidget() override;
   DerivedProductsWidget(const DerivedProductsWidget&)            = delete;
   DerivedProductsWidget(DerivedProductsWidget&&)                 = delete;
   DerivedProductsWidget& operator=(const DerivedProductsWidget&) = delete;
   DerivedProductsWidget& operator=(DerivedProductsWidget&&)      = delete;

   void showEvent(QShowEvent* event) override;

   void UpdateAvailableProducts(
      const common::Level3ProductCategoryMap& updatedCategoryMap);
   void UpdateProductSelection(common::RadarProductGroup group,
                               const std::string&        productName);

signals:
   void RadarProductSelected(common::RadarProductGroup group,
                             const std::string&        productName,
                             int16_t                   productCode);

private:
   std::shared_ptr<DerivedProductsWidgetImpl> p;
};

} // namespace scwx::qt::ui
