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

   bool SetValueOrDefault(const Container& c) override;

   bool Validate(const Container& c) const override;
   bool ValidateElement(const T& value) const;

   T GetElementDefault() const;

   void SetElementDefault(const T& value);
   void SetElementMinimum(const T& value);
   void SetElementMaximum(const T& value);
   void SetElementValidator(std::function<bool(const T&)> validator);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

#ifdef SETTINGS_CONTAINER_IMPLEMENTATION
template class SettingsContainer<std::vector<int64_t>>;
#endif

} // namespace settings
} // namespace qt
} // namespace scwx
