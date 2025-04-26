#include "radar_product_selector_widget.hpp"
#include <QLabel>

namespace scwx {
namespace qt {
namespace ui {

RadarProductSelectorWidget::RadarProductSelectorWidget(QWidget* parent) :
    QWidget(parent),
    isExpanded_(true)
{
   QVBoxLayout* mainLayout = new QVBoxLayout(this);
   mainLayout->setSpacing(0);
   mainLayout->setContentsMargins(0, 0, 0, 0);

   toggleButton_ = new QPushButton("Radar Product ▾");
   toggleButton_->setCheckable(true);
   toggleButton_->setChecked(true);
   toggleButton_->setStyleSheet("QPushButton { text-align: left; padding: 6px; font-size: 14px; }");

   connect(toggleButton_, &QPushButton::clicked, this, &RadarProductSelectorWidget::TogglePanel);
   mainLayout->addWidget(toggleButton_);

   productPanel_ = new QWidget();
   panelLayout_ = new QVBoxLayout(productPanel_);
   productPanel_->setStyleSheet("background-color: #202020; border: 1px solid #444;");

   // Example entries
   QStringList products = {"Reflectivity", "Velocity", "Correlation Coefficient"};
   for (const QString& p : products)
   {
      QLabel* label = new QLabel(p);
      label->setStyleSheet("padding: 4px; font-size: 13px;");
      panelLayout_->addWidget(label);
   }

   productPanel_->setMaximumHeight(100); // Show some items
   mainLayout->addWidget(productPanel_);

   // Animation
   animation_ = new QPropertyAnimation(productPanel_, "maximumHeight");
   animation_->setDuration(250);
}

void RadarProductSelectorWidget::TogglePanel()
{
   isExpanded_ = !isExpanded_;

   int start = isExpanded_ ? 0 : productPanel_->height();
   int end   = isExpanded_ ? productPanel_->sizeHint().height() : 0;

   animation_->stop();
   animation_->setStartValue(start);
   animation_->setEndValue(end);
   animation_->start();

   toggleButton_->setText(isExpanded_ ? "Radar Product ▾" : "Radar Product ▸");
}

} // namespace ui
} // namespace qt
} // namespace scwx
