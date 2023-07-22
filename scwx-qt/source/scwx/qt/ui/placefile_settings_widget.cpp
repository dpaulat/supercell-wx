#include "placefile_settings_widget.hpp"
#include "ui_placefile_settings_widget.h"

namespace scwx
{
namespace qt
{
namespace ui
{

class PlacefileSettingsWidgetImpl
{
public:
   explicit PlacefileSettingsWidgetImpl(PlacefileSettingsWidget* self) :
       self_ {self}
   {
   }
   ~PlacefileSettingsWidgetImpl() = default;

   PlacefileSettingsWidget* self_;
};

PlacefileSettingsWidget::PlacefileSettingsWidget(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<PlacefileSettingsWidgetImpl>(this)},
    ui(new Ui::PlacefileSettingsWidget)
{
   ui->setupUi(this);
}

PlacefileSettingsWidget::~PlacefileSettingsWidget()
{
   delete ui;
}

} // namespace ui
} // namespace qt
} // namespace scwx
