#include <scwx/qt/ui/settings/hotkey_settings_widget.hpp>
#include <scwx/qt/ui/hotkey_edit.hpp>
#include <scwx/qt/settings/hotkey_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>
#include <scwx/qt/types/hotkey_types.hpp>

#include <boost/unordered/unordered_flat_map.hpp>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

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
   explicit Impl(HotkeySettingsWidget* self)
   {
      auto& hotkeySettings = settings::HotkeySettings::Instance();

      gridLayout_ = new QGridLayout(self);
      contents_   = new QWidget(self);
      contents_->setLayout(gridLayout_);

      scrollArea_ = new QScrollArea(self);
      scrollArea_->setHorizontalScrollBarPolicy(
         Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
      scrollArea_->setWidgetResizable(true);
      scrollArea_->setWidget(contents_);

      layout_ = new QVBoxLayout(self);
      layout_->setContentsMargins(0, 0, 0, 0);
      layout_->addWidget(scrollArea_);

      self->setLayout(layout_);

      int row = 0;

      for (types::Hotkey hotkey : types::HotkeyIterator())
      {
         const std::string& labelText = types::GetHotkeyLongName(hotkey);

         QLabel*      label = new QLabel(QObject::tr(labelText.c_str()), self);
         HotkeyEdit*  hotkeyEdit  = new HotkeyEdit(self);
         QToolButton* resetButton = new QToolButton(self);

         resetButton->setIcon(
            QIcon {":/res/icons/font-awesome-6/rotate-left-solid.svg"});
         resetButton->setVisible(false);

         gridLayout_->addWidget(label, row, 0);
         gridLayout_->addWidget(hotkeyEdit, row, 1);
         gridLayout_->addWidget(resetButton, row, 2);

         // Create settings interface
         auto result = hotkeys_.emplace(
            hotkey, settings::SettingsInterface<std::string> {});
         auto& pair      = *result.first;
         auto& interface = pair.second;

         // Add to settings list
         self->AddSettingsInterface(&interface);

         auto& hotkeyVariable = hotkeySettings.hotkey(hotkey);
         interface.SetSettingsVariable(hotkeyVariable);
         interface.SetEditWidget(hotkeyEdit);
         interface.SetResetButton(resetButton);

         ++row;
      }

      QSpacerItem* spacer =
         new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
      gridLayout_->addItem(spacer, row, 0);
   }
   ~Impl() = default;

   QWidget*     contents_;
   QLayout*     layout_;
   QScrollArea* scrollArea_ {};
   QGridLayout* gridLayout_ {};

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
