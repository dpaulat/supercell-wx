#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_interface_base.hpp>

#include <QWidget>

#define SCWX_ENUM_MAP_FROM_VALUE(Iterator, ToName)                             \
   [](const std::string& text) -> std::string                                  \
   {                                                                           \
      for (auto enumValue : Iterator)                                          \
      {                                                                        \
         const std::string enumName = ToName(enumValue);                       \
                                                                               \
         if (boost::iequals(text, enumName))                                   \
         {                                                                     \
            /* Return label */                                                 \
            return enumName;                                                   \
         }                                                                     \
      }                                                                        \
                                                                               \
      /* Label not found, return unknown */                                    \
      return "?";                                                              \
   }

#define SCWX_SETTINGS_COMBO_BOX(settingsInterface, comboBox, Iterator, ToName) \
   for (const auto& enumValue : Iterator)                                      \
   {                                                                           \
      comboBox->addItem(QString::fromStdString(ToName(enumValue)));            \
   }                                                                           \
                                                                               \
   settingsInterface.SetMapFromValueFunction(                                  \
      SCWX_ENUM_MAP_FROM_VALUE(Iterator, ToName));                             \
   settingsInterface.SetMapToValueFunction(                                    \
      [](std::string text) -> std::string                                      \
      {                                                                        \
         boost::to_lower(text);                                                \
         return text;                                                          \
      });                                                                      \
                                                                               \
   settingsInterface.SetEditWidget(comboBox);

namespace scwx
{
namespace qt
{
namespace ui
{

class SettingsPageWidget : public QWidget
{
   Q_OBJECT

public:
   explicit SettingsPageWidget(QWidget* parent = nullptr);
   ~SettingsPageWidget();

   bool CommitChanges();
   void DiscardChanges();
   void ResetToDefault();

protected:
   void AddSettingsCategory(settings::SettingsCategory* category);
   void AddSettingsInterface(settings::SettingsInterfaceBase* setting);

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
