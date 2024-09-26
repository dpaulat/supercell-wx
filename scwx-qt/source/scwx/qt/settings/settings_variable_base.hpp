#pragma once

#include <memory>
#include <string>

#include <boost/json/object.hpp>
#include <boost/signals2/signal.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

/**
 * @brief Settings Variable base class
 */
class SettingsVariableBase
{
protected:
   explicit SettingsVariableBase(const std::string& name);
   virtual ~SettingsVariableBase();

public:
   SettingsVariableBase(const SettingsVariableBase&)            = delete;
   SettingsVariableBase& operator=(const SettingsVariableBase&) = delete;

   SettingsVariableBase(SettingsVariableBase&&) noexcept;
   SettingsVariableBase& operator=(SettingsVariableBase&&) noexcept;

   std::string name() const;

   /**
    * Gets the signal invoked when the settings variable is changed.
    *
    * @return Changed signal
    */
   boost::signals2::signal<void()>& changed_signal();

   /**
    * Gets the signal invoked when the settings variable is staged.
    *
    * @return Staged signal
    */
   boost::signals2::signal<void()>& staged_signal();

   /**
    * Sets the current value of the settings variable to default.
    */
   virtual void SetValueToDefault() = 0;

   /**
    * Stages the default value of the settings variable.
    */
   virtual void StageDefault() = 0;

   /**
    * Sets the current value of the settings variable to the staged value.
    *
    * @return true if the staged value was committed, false if no staged value
    * is present.
    */
   virtual bool Commit() = 0;

   /**
    * Clears the staged value of the settings variable.
    */
   virtual void Reset() = 0;

   /**
    * Reads the value from the JSON object. If the read value is out of range,
    * the value is set to the minimum or maximum. If the read value fails
    * validation, the value is set to default.
    *
    * @param json JSON object to read
    *
    * @return true if the read value is valid, false if the value was modified.
    */
   virtual bool ReadValue(const boost::json::object& json) = 0;

   /**
    * Writes the current value to the JSON object.
    *
    * @param json JSON object to write
    */
   virtual void WriteValue(boost::json::object& json) const = 0;

protected:
   friend bool  operator==(const SettingsVariableBase& lhs,
                          const SettingsVariableBase& rhs);
   virtual bool Equals(const SettingsVariableBase& o) const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

bool operator==(const SettingsVariableBase& lhs,
                const SettingsVariableBase& rhs);

} // namespace settings
} // namespace qt
} // namespace scwx
