#include <scwx/qt/ui/settings/settings_page_widget.hpp>
#include <scwx/qt/settings/settings_interface_base.hpp>

#include <vector>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ =
   "scwx::qt::ui::settings::settings_page_widget";

class SettingsPageWidget::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;

   std::vector<settings::SettingsInterfaceBase*> settings_;
};

SettingsPageWidget::SettingsPageWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Impl>()}
{
}

SettingsPageWidget::~SettingsPageWidget() = default;

void SettingsPageWidget::AddSettingsInterface(
   settings::SettingsInterfaceBase* setting)
{
   p->settings_.push_back(setting);
}

bool SettingsPageWidget::CommitChanges()
{
   bool committed = false;

   for (auto& setting : p->settings_)
   {
      committed |= setting->Commit();
   }

   return committed;
}

void SettingsPageWidget::DiscardChanges()
{
   for (auto& setting : p->settings_)
   {
      setting->Reset();
   }
}

void SettingsPageWidget::ResetToDefault()
{
   for (auto& setting : p->settings_)
   {
      setting->StageDefault();
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
