#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class QAbstractButton;
class QWidget;

namespace scwx
{
namespace qt
{
namespace settings
{

template<class T>
class SettingsVariable;

template<class T>
class SettingsInterface
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
    * set prior to setting any widgets.
    *
    * @param variable Settings variable
    */
   void SetSettingsVariable(SettingsVariable<T>& variable);

   /**
    * Sets the edit widget from the settings dialog.
    *
    * @param widget Edit widget
    */
   void SetEditWidget(QWidget* widget);

   /**
    * Sets the reset button from the settings dialog.
    *
    * @param button Reset button
    */
   void SetResetButton(QAbstractButton* button);

   /**
    * If the edit widget displays a different value than what is stored in the
    * settings variable, a mapping function must be provided in order to convert
    * the value used by the edit widget from the settings value.
    *
    * @param function Map from settings value function
    */
   void SetMapFromValueFunction(std::function<T(const T&)> function);

   /**
    * If the edit widget displays a different value than what is stored in the
    * settings variable, a mapping function must be provided in order to convert
    * the value used by the edit widget to the settings value.
    *
    * @param function Map to settings value function
    */
   void SetMapToValueFunction(std::function<T(const T&)> function);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

#ifdef SETTINGS_INTERFACE_IMPLEMENTATION
template class SettingsInterface<bool>;
template class SettingsInterface<std::int64_t>;
template class SettingsInterface<std::string>;

// Containers are not to be used directly
template class SettingsInterface<std::vector<std::int64_t>>;
#endif

} // namespace settings
} // namespace qt
} // namespace scwx
