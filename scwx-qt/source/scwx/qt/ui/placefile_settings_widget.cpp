#include "placefile_settings_widget.hpp"
#include "ui_placefile_settings_widget.h"

#include <scwx/qt/ui/open_url_dialog.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::placefile_settings_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileSettingsWidgetImpl
{
public:
   explicit PlacefileSettingsWidgetImpl(PlacefileSettingsWidget* self) :
       self_ {self}
   {
   }
   ~PlacefileSettingsWidgetImpl() = default;

   void ConnectSignals();

   PlacefileSettingsWidget* self_;
   OpenUrlDialog*           openUrlDialog_ {nullptr};
};

PlacefileSettingsWidget::PlacefileSettingsWidget(QWidget* parent) :
    QFrame(parent),
    p {std::make_unique<PlacefileSettingsWidgetImpl>(this)},
    ui(new Ui::PlacefileSettingsWidget)
{
   ui->setupUi(this);

   p->openUrlDialog_ = new OpenUrlDialog("Add Placefile", this);

   p->ConnectSignals();
}

PlacefileSettingsWidget::~PlacefileSettingsWidget()
{
   delete ui;
}

void PlacefileSettingsWidgetImpl::ConnectSignals()
{
   QObject::connect(self_->ui->addButton,
                    &QPushButton::clicked,
                    self_,
                    [this]() { openUrlDialog_->open(); });

   QObject::connect(
      openUrlDialog_,
      &OpenUrlDialog::accepted,
      self_,
      [this]()
      { logger_->info("Add URL: {}", openUrlDialog_->url().toStdString()); });
}

} // namespace ui
} // namespace qt
} // namespace scwx
