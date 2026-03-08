#pragma once

#include <memory>
#include <string>

#include <boost/json/object.hpp>
#include <boost/signals2/signal.hpp>

namespace scwx::qt::settings
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

   [[nodiscard]] std::string name() const;

   /**
    * Gets whether or not the settings variable is currently set to its default
    * value.
    *
    * @return true if the settings variable is currently set to its default
    * value, otherwise false.
    */
   [[nodiscard]] virtual bool IsDefault() const = 0;

   /**
    * Gets whether or not the settings variable currently has its staged value
    * set to default.
    *
    * @return true if the settings variable currently has its staged value set
    * to default, otherwise false.
    */
   [[nodiscard]] virtual bool IsDefaultStaged() const = 0;

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

   /**
    * Connects a slot to be called when the settings variable changes.
    *
    * @param slot Slot to connect
    *
    * @return Connection object
    */
   virtual boost::signals2::connection
   ConnectChanged(std::function<void()> slot) = 0;

   /**
    * Connects a slot to be called when the settings variable is staged.
    *
    * @param slot Slot to connect
    *
    * @return Connection object
    */
   virtual boost::signals2::connection
   ConnectStaged(std::function<void()> slot) = 0;

protected:
   friend bool                operator==(const SettingsVariableBase& lhs,
                          const SettingsVariableBase& rhs);
   [[nodiscard]] virtual bool Equals(const SettingsVariableBase& o) const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

bool operator==(const SettingsVariableBase& lhs,
                const SettingsVariableBase& rhs);

} // namespace scwx::qt::settings
