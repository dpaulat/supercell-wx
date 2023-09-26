#pragma once

#include <scwx/qt/settings/settings_interface_base.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace qt
{
namespace settings
{

template<class T>
class SettingsVariable;

template<class T>
class SettingsInterface : public SettingsInterfaceBase
{
public:
   explicit SettingsInterface();
   ~SettingsInterface();

   SettingsInterface(const SettingsInterface&)            = delete;
   SettingsInterface& operator=(const SettingsInterface&) = delete;

   SettingsInterface(SettingsInterface&&) noexcept;
   SettingsInterface& operator=(SettingsInterface&&) noexcept;

   /**
    * Sets the settings variable associated with the interface. This must be
    * set prior to calling any other functions.
    *
    * @param variable Settings variable
    */
   void SetSettingsVariable(SettingsVariable<T>& variable);

   /**
    * Gets the settings variable associated with the interface.
    *
    * @return Settings variable
    */
   SettingsVariable<T>* GetSettingsVariable() const;

   /**
    * Sets the current value of the associated settings variable to the staged
    * value.
    *
    * @return true if the staged value was committed, false if no staged value
    * is present.
    */
   bool Commit() override;

   /**
    * Clears the staged value of the associated settings variable.
    */
   void Reset() override;

   /**
    * Stages the default value of the associated settings variable.
    */
   void StageDefault() override;

   /**
    * Sets the edit widget from the settings dialog.
    *
    * @param widget Edit widget
    */
   void SetEditWidget(QWidget* widget) override;

   /**
    * Sets the reset button from the settings dialog.
    *
    * @param button Reset button
    */
   void SetResetButton(QAbstractButton* button) override;

   /**
    * If the edit widget displays a different value than what is stored in the
    * settings variable, a mapping function must be provided in order to convert
    * the value used by the edit widget from the settings value.
    *
    * @param function Map from settings value function
    */
   void SetMapFromValueFunction(std::function<std::string(const T&)> function);

   /**
    * If the edit widget displays a different value than what is stored in the
    * settings variable, a mapping function must be provided in order to convert
    * the value used by the edit widget to the settings value.
    *
    * @param function Map to settings value function
    */
   void SetMapToValueFunction(std::function<T(const std::string&)> function);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

#ifdef SETTINGS_INTERFACE_IMPLEMENTATION
template class SettingsInterface<bool>;
template class SettingsInterface<double>;
template class SettingsInterface<std::int64_t>;
template class SettingsInterface<std::string>;

// Containers are not to be used directly
template class SettingsInterface<std::vector<std::int64_t>>;
#endif

} // namespace settings
} // namespace qt
} // namespace scwx
