#include <scwx/qt/ui/settings/hotkey_settings_widget.hpp>
#include <scwx/qt/ui/hotkey_edit.hpp>
#include <scwx/qt/settings/hotkey_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/types/hotkey_types.hpp>

#include <boost/unordered/unordered_flat_map.hpp>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ =
   "scwx::qt::ui::settings::hotkey_settings_widget";

class HotkeySettingsWidget::Impl
{
public:
   explicit Impl(HotkeySettingsWidget* self) :
       self_ {self}, layout_ {new QGridLayout(self)}
   {
      auto& hotkeySettings = settings::HotkeySettings::Instance();

      layout_->setContentsMargins(0, 0, 0, 0);

      int row = 0;

      for (types::Hotkey hotkey : types::HotkeyIterator())
      {
         const std::string& labelText = types::GetHotkeyLongName(hotkey);

         QLabel*      label = new QLabel(QObject::tr(labelText.c_str()), self);
         HotkeyEdit*  hotkeyEdit  = new HotkeyEdit(self);
         QToolButton* resetButton = new QToolButton(self_);

         resetButton->setIcon(
            QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
         resetButton->setVisible(false);

         layout_->addWidget(label, row, 0);
         layout_->addWidget(hotkeyEdit, row, 1);
         layout_->addWidget(resetButton, row, 2);

         // Create settings interface
         auto result = hotkeys_.emplace(
            hotkey, settings::SettingsInterface<std::string> {});
         auto& pair      = *result.first;
         auto& interface = pair.second;

         // Add to settings list
         self_->AddSettingsInterface(&interface);

         auto& hotkeyVariable = hotkeySettings.hotkey(hotkey);
         interface.SetSettingsVariable(hotkeyVariable);
         interface.SetEditWidget(hotkeyEdit);
         interface.SetResetButton(resetButton);

         ++row;
      }

      QSpacerItem* spacer =
         new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
      layout_->addItem(spacer, row, 0);
   }
   ~Impl() = default;

   HotkeySettingsWidget* self_;
   QGridLayout*          layout_;

   boost::unordered_flat_map<types::Hotkey,
                             settings::SettingsInterface<std::string>>
      hotkeys_ {};
};

HotkeySettingsWidget::HotkeySettingsWidget(QWidget* parent) :
    SettingsPageWidget(parent), p {std::make_shared<Impl>(this)}
{
}

HotkeySettingsWidget::~HotkeySettingsWidget() = default;

} // namespace ui
} // namespace qt
} // namespace scwx
