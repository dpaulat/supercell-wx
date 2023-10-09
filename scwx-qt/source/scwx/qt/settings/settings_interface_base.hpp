#pragma once

#include <memory>

class QAbstractButton;
class QWidget;

namespace scwx
{
namespace qt
{
namespace settings
{

class SettingsInterfaceBase
{
public:
   explicit SettingsInterfaceBase();
   ~SettingsInterfaceBase();

   SettingsInterfaceBase(const SettingsInterfaceBase&)            = delete;
   SettingsInterfaceBase& operator=(const SettingsInterfaceBase&) = delete;

   SettingsInterfaceBase(SettingsInterfaceBase&&) noexcept;
   SettingsInterfaceBase& operator=(SettingsInterfaceBase&&) noexcept;

   /**
    * Gets whether the staged value (or current value, if none staged) is
    * set to the default value.
    *
    * @return true if the settings variable is set to default, otherwise false.
    */
   virtual bool IsDefault() = 0;

   /**
    * Sets the current value of the associated settings variable to the staged
    * value.
    *
    * @return true if the staged value was committed, false if no staged value
    * is present.
    */
   virtual bool Commit() = 0;

   /**
    * Clears the staged value of the associated settings variable.
    */
   virtual void Reset() = 0;

   /**
    * Stages the default value of the associated settings variable.
    */
   virtual void StageDefault() = 0;

   /**
    * Sets the edit widget from the settings dialog.
    *
    * @param widget Edit widget
    */
   virtual void SetEditWidget(QWidget* widget) = 0;

   /**
    * Sets the reset button from the settings dialog.
    *
    * @param button Reset button
    */
   virtual void SetResetButton(QAbstractButton* button) = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
