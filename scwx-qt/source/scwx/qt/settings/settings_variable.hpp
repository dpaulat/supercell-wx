#pragma once

#include <scwx/qt/settings/settings_variable_base.hpp>

#include <functional>
#include <optional>

class QAbstractButton;
class QWidget;

namespace scwx
{
namespace qt
{
namespace settings
{

template<class T>
class SettingsVariable : public SettingsVariableBase
{
public:
   /**
    * Callback function for when a value changes.
    *
    * @param value New value
    */
   typedef std::function<void(const T& value)> ValueCallbackFunction;

   explicit SettingsVariable(const std::string& name);
   ~SettingsVariable();

   SettingsVariable(const SettingsVariable&)            = delete;
   SettingsVariable& operator=(const SettingsVariable&) = delete;

   SettingsVariable(SettingsVariable&&) noexcept;
   SettingsVariable& operator=(SettingsVariable&&) noexcept;

   /**
    * Gets the current value of the settings variable.
    *
    * @return Current value
    */
   T GetValue() const;

   /**
    * Sets the current value of the settings variable.
    *
    * @param value Value to set
    *
    * @return true if the current value was set, false if the value failed
    * validation.
    */
   bool SetValue(const T& value);

   /**
    * Sets the current value of the settings variable. If the value is out of
    * range, the value is set to the minimum or maximum. If the value fails
    * validation, the value is set to default.
    *
    * @param value Value to set
    *
    * @return true if the value is valid, false if the value was modified.
    */
   virtual bool SetValueOrDefault(const T& value);

   /**
    * Sets the current value of the settings variable to default.
    */
   void SetValueToDefault() override;

   /**
    * Stages the default value of the settings variable.
    */
   void StageDefault() override;

   /**
    * Sets the staged value of the settings variable.
    *
    * @param value Value to stage
    *
    * @return true if the staged value was set, false if the value failed
    * validation.
    */
   bool StageValue(const T& value);

   /**
    * Sets the current value of the settings variable to the staged value.
    *
    * @return true if the staged value was committed, false if no staged value
    * is present.
    */
   bool Commit();

   /**
    * Clears the staged value of the settings variable.
    */
   void Reset();

   /**
    * Gets the staged value of the settings variable, if defined.
    *
    * @return Optional staged value
    */
   std::optional<T> GetStaged() const;

   /**
    * Validate the value against the defined parameters of the settings
    * variable.
    *
    * @param value Value to validate
    *
    * @return true if the value is valid, false if the value failed validation.
    */
   virtual bool Validate(const T& value) const;

   /**
    * Gets the default value of the settings variable.
    *
    * @return Default value
    */
   T GetDefault() const;

   /**
    * Sets the default value of the settings variable.
    *
    * @param value Default value
    */
   void SetDefault(const T& value);

   /**
    * Gets the minimum value of the settings variable, if defined.
    *
    * @return Optional minimum value
    */
   std::optional<T> GetMinimum() const;

   /**
    * Gets the maximum value of the settings variable, if defined.
    *
    * @return Optional maximum value
    */
   std::optional<T> GetMaximum() const;

   /**
    * Sets the minimum value of the settings variable.
    *
    * @param value Minimum value
    */
   void SetMinimum(const T& value);

   /**
    * Sets the maximum value of the settings variable.
    *
    * @param value Maximum value
    */
   void SetMaximum(const T& value);

   /**
    * Sets a custom validator function for the settings variable.
    *
    * @param validator Validator function
    */
   void SetValidator(std::function<bool(const T&)> validator);

   /**
    * Reads the value from the JSON object. If the read value is out of range,
    * the value is set to the minimum or maximum. If the read value fails
    * validation, the value is set to default.
    *
    * @param json JSON object to read
    *
    * @return true if the read value is valid, false if the value was modified.
    */
   virtual bool ReadValue(const boost::json::object& json) override;

   /**
    * Writes the current value to the JSON object.
    *
    * @param json JSON object to write
    */
   virtual void WriteValue(boost::json::object& json) const override;

   /**
    * Registers a function to be called when the current value changes. The
    * current value is passed to the callback function.
    *
    * @param callback Function to be called
    */
   void RegisterValueChangedCallback(ValueCallbackFunction callback);

   /**
    * Registers a function to be called when the staged value changes (or a
    * value is unstaged or committed). The staged value is passed to the
    * callback function (or in the case of unstaging or committing, the current
    * value).
    *
    * @param callback Function to be called
    */
   void RegisterValueStagedCallback(ValueCallbackFunction callback);

protected:
   virtual bool Equals(const SettingsVariableBase& o) const override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

#ifdef SETTINGS_VARIABLE_IMPLEMENTATION
template class SettingsVariable<bool>;
template class SettingsVariable<double>;
template class SettingsVariable<std::int64_t>;
template class SettingsVariable<std::string>;

// Containers are not to be used directly
template class SettingsVariable<std::vector<std::int64_t>>;
#endif

} // namespace settings
} // namespace qt
} // namespace scwx
