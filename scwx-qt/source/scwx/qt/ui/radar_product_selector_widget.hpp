#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPropertyAnimation>

namespace scwx {
namespace qt {
namespace ui {

class RadarProductSelectorWidget : public QWidget
{
   Q_OBJECT

public:
   explicit RadarProductSelectorWidget(QWidget* parent = nullptr);
   ~RadarProductSelectorWidget() = default;

private:
   QPushButton*        toggleButton_;
   QWidget*            productPanel_;
   QVBoxLayout*        panelLayout_;
   QPropertyAnimation* animation_;
   bool                isExpanded_;

   void TogglePanel();
};

} // namespace ui
} // namespace qt
} // namespace scwx
