#pragma once

#include <functional>
#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

template<class T>
class SettingsVariable
{
public:
   explicit SettingsVariable(const std::string& name);
   ~SettingsVariable();

   SettingsVariable(const SettingsVariable&)            = delete;
   SettingsVariable& operator=(const SettingsVariable&) = delete;

   SettingsVariable(SettingsVariable&&) noexcept;
   SettingsVariable& operator=(SettingsVariable&&) noexcept;

   std::string name() const;

   T    GetValue() const;
   bool SetValue(const T& value);
   bool SetValueOrDefault(const T& value);

   bool StageValue(const T& value);
   void Commit();

   T GetDefault() const;

   void SetDefault(const T& value);
   void SetMinimum(const T& value);
   void SetMaximum(const T& value);
   void SetValidator(std::function<bool(const T&)> validator);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

// Instantiated templates:
//    template class SettingsVariable<bool>;
//    template class SettingsVariable<int64_t>;
//    template class SettingsVariable<std::string>;
//    template class SettingsVariable<std::vector<int64_t>>;

} // namespace settings
} // namespace qt
} // namespace scwx
