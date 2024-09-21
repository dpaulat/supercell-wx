#pragma once

#include <scwx/qt/settings/settings_variable_base.hpp>

#include <memory>
#include <string>

#include <boost/json/object.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

class SettingsCategory
{
public:
   explicit SettingsCategory(const std::string& name);
   virtual ~SettingsCategory();

   SettingsCategory(const SettingsCategory&)            = delete;
   SettingsCategory& operator=(const SettingsCategory&) = delete;

   SettingsCategory(SettingsCategory&&) noexcept;
   SettingsCategory& operator=(SettingsCategory&&) noexcept;

   std::string name() const;

   /**
    * Set all variables to their defaults.
    */
   void SetDefaults();

   /**
    * Reads the variables from the JSON object.
    *
    * @param json JSON object to read
    *
    * @return true if the values read are valid, false if any values were
    * modified.
    */
   virtual bool ReadJson(const boost::json::object& json);

   /**
    * Writes the variables to the JSON object.
    *
    * @param json JSON object to write
    */
   virtual void WriteJson(boost::json::object& json) const;

   void RegisterSubcategory(SettingsCategory& subcategory);
   void RegisterSubcategoryArray(const std::string&             name,
                                 std::vector<SettingsCategory>& subcategories);
   void RegisterSubcategoryArray(const std::string&              name,
                                 std::vector<SettingsCategory*>& subcategories);
   void
   RegisterVariables(std::initializer_list<SettingsVariableBase*> variables);
   void RegisterVariables(std::vector<SettingsVariableBase*> variables);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
