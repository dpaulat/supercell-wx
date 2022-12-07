#define SETTINGS_CONTAINER_IMPLEMENTATION

#include <scwx/qt/settings/settings_container.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::settings_container";

template<class Container>
class SettingsContainer<Container>::Impl
{
public:
   explicit Impl() {}

   ~Impl() {}

   T                             elementDefault_ {};
   std::optional<T>              elementMinimum_ {};
   std::optional<T>              elementMaximum_ {};
   std::function<bool(const T&)> elementValidator_ {nullptr};
};

template<class Container>
SettingsContainer<Container>::SettingsContainer(const std::string& name) :
    SettingsVariable<Container>(name), p(std::make_unique<Impl>())
{
}
template<class Container>
SettingsContainer<Container>::~SettingsContainer() = default;

template<class Container>
SettingsContainer<Container>::SettingsContainer(SettingsContainer&&) noexcept =
   default;
template<class Container>
SettingsContainer<Container>&
SettingsContainer<Container>::operator=(SettingsContainer&&) noexcept = default;

template<class Container>
bool SettingsContainer<Container>::SetValueOrDefault(const Container& c)
{
   bool validated = true;

   Container validatedValues;
   validatedValues.reserve(c.size());

   std::transform(
      c.cbegin(),
      c.cend(),
      std::back_inserter(validatedValues),
      [&](auto& value)
      {
         if (ValidateElement(value))
         {
            return value;
         }
         else if (p->elementMinimum_.has_value() && value < p->elementMinimum_)
         {
            validated = false;
            return *p->elementMinimum_;
         }
         else if (p->elementMaximum_.has_value() && value > p->elementMaximum_)
         {
            validated = false;
            return *p->elementMaximum_;
         }
         else
         {
            validated = false;
            return p->elementDefault_;
         }
      });

   return SettingsVariable<Container>::SetValueOrDefault(validatedValues) &&
          validated;
}

template<class Container>
SettingsContainer<Container>::T
SettingsContainer<Container>::GetElementDefault() const
{
   return p->elementDefault_;
}

template<class Container>
void SettingsContainer<Container>::SetElementDefault(const T& value)
{
   p->elementDefault_ = value;
}

template<class Container>
void SettingsContainer<Container>::SetElementMinimum(const T& value)
{
   p->elementMinimum_ = value;
}

template<class Container>
void SettingsContainer<Container>::SetElementMaximum(const T& value)
{
   p->elementMaximum_ = value;
}

template<class Container>
void SettingsContainer<Container>::SetElementValidator(
   std::function<bool(const T&)> validator)
{
   p->elementValidator_ = validator;
}

template<class Container>
bool SettingsContainer<Container>::Validate(const Container& c) const
{
   if (!SettingsVariable<Container>::Validate(c))
   {
      return false;
   }

   for (auto& element : c)
   {
      if (!ValidateElement(element))
      {
         return false;
      }
   }

   return true;
}

template<class Container>
bool SettingsContainer<Container>::ValidateElement(const T& value) const
{
   return (
      // Validate minimum
      (!p->elementMinimum_.has_value() || value >= p->elementMinimum_) &&
      // Validate maximum
      (!p->elementMaximum_.has_value() || value <= p->elementMaximum_) &&
      // User-validation
      (p->elementValidator_ == nullptr || p->elementValidator_(value)));
}

} // namespace settings
} // namespace qt
} // namespace scwx
