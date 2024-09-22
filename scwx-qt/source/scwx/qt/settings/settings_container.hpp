#pragma once

#include <scwx/qt/settings/settings_variable.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

template<class Container>
class SettingsContainer : public SettingsVariable<Container>
{
public:
   using T = Container::value_type;

   explicit SettingsContainer(const std::string& name);
   ~SettingsContainer();

   SettingsContainer(const SettingsContainer&)            = delete;
   SettingsContainer& operator=(const SettingsContainer&) = delete;

   SettingsContainer(SettingsContainer&&) noexcept;
   SettingsContainer& operator=(SettingsContainer&&) noexcept;

   /**
    * Sets the current value of the settings variable. If the value is out of
    * range, the value is set to the minimum or maximum. If the value fails
    * validation, the value is set to default.
    *
    * @param c Container value to set
    *
    * @return true if the value is valid, false if the value was modified.
    */
   bool SetValueOrDefault(const Container& c) override;

   /**
    * Validate the container value against the defined parameters of the
    * settings variable.
    *
    * @param c Container value to validate
    *
    * @return true if the value is valid, false if the value failed validation.
    */
   bool Validate(const Container& c) const override;

   /**
    * Validate the element value against the defined parameters of the settings
    * variable.
    *
    * @param value Element value to validate
    *
    * @return true if the value is valid, false if the value failed validation.
    */
   bool ValidateElement(const T& value) const;

   /**
    * Gets the default element value of the settings variable.
    *
    * @return Default element value
    */
   T GetElementDefault() const;

   /**
    * Sets the default element value of the settings variable.
    *
    * @param value Default element value
    */
   void SetElementDefault(const T& value);

   /**
    * Sets the minimum element value of the settings variable.
    *
    * @param value Minimum element value
    */
   void SetElementMinimum(const T& value);

   /**
    * Sets the maximum element value of the settings variable.
    *
    * @param value Maximum element value
    */
   void SetElementMaximum(const T& value);

   /**
    * Sets a custom validator function for each element of the settings
    * variable.
    *
    * @param validator Element validator function
    */
   void SetElementValidator(std::function<bool(const T&)> validator);

protected:
   virtual bool Equals(const SettingsVariableBase& o) const override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
