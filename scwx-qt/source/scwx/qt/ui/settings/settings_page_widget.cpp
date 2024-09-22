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

   std::vector<settings::SettingsCategory*>      categories_;
   std::vector<settings::SettingsInterfaceBase*> settings_;
};

SettingsPageWidget::SettingsPageWidget(QWidget* parent) :
    QWidget(parent), p {std::make_shared<Impl>()}
{
}

SettingsPageWidget::~SettingsPageWidget() = default;

void SettingsPageWidget::AddSettingsCategory(
   settings::SettingsCategory* category)
{
   p->categories_.push_back(category);
}

void SettingsPageWidget::AddSettingsInterface(
   settings::SettingsInterfaceBase* setting)
{
   p->settings_.push_back(setting);
}

bool SettingsPageWidget::CommitChanges()
{
   bool committed = false;

   for (auto& category : p->categories_)
   {
      committed |= category->Commit();
   }

   for (auto& setting : p->settings_)
   {
      committed |= setting->Commit();
   }

   return committed;
}

void SettingsPageWidget::DiscardChanges()
{
   for (auto& category : p->categories_)
   {
      category->Reset();
   }

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
