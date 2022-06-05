#pragma once

#include <scwx/common/products.hpp>

#include <QWidget>

namespace scwx
{
namespace qt
{
namespace ui
{

class Level2ProductsWidgetImpl;

class Level2ProductsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit Level2ProductsWidget(QWidget* parent = nullptr);
   ~Level2ProductsWidget();

   void showEvent(QShowEvent* event) override;

   void UpdateProductSelection(common::RadarProductGroup group,
                               const std::string&        productName);

signals:
   void RadarProductSelected(common::RadarProductGroup group,
                             const std::string&        productName,
                             int16_t                   productCode);

private:
   std::shared_ptr<Level2ProductsWidgetImpl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
