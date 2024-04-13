#pragma once

#include <scwx/qt/settings/settings_interface_base.hpp>

#include <QWidget>

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
   void AddSettingsInterface(settings::SettingsInterfaceBase* setting);

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
