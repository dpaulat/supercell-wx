#define SETTINGS_VARIABLE_IMPLEMENTATION

#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::settings_variable";

template<class T>
class SettingsVariable<T>::Impl
{
public:
   explicit Impl(const std::string& name) : name_ {name} {}

   ~Impl() {}

   bool Validate(const T& value);

   const std::string             name_;
   T                             value_ {};
   T                             default_ {};
   std::optional<T>              staged_ {};
   std::optional<T>              minimum_ {};
   std::optional<T>              maximum_ {};
   std::function<bool(const T&)> validator_ {nullptr};
};

template<class T>
SettingsVariable<T>::SettingsVariable(const std::string& name) :
    p(std::make_unique<Impl>(name))
{
}
template<class T>
SettingsVariable<T>::~SettingsVariable() = default;

template<class T>
SettingsVariable<T>::SettingsVariable(SettingsVariable&&) noexcept = default;
template<class T>
SettingsVariable<T>&
SettingsVariable<T>::operator=(SettingsVariable&&) noexcept = default;

template<class T>
std::string SettingsVariable<T>::name() const
{
   return p->name_;
}

template<class T>
T SettingsVariable<T>::GetValue() const
{
   return p->value_;
}

template<class T>
bool SettingsVariable<T>::SetValue(const T& value)
{
   bool validated = false;

   if (p->Validate(value))
   {
      p->value_ = value;
      validated = true;
   }

   return validated;
}

template<class T>
bool SettingsVariable<T>::SetValueOrDefault(const T& value)
{
   bool validated = false;

   if (p->Validate(value))
   {
      p->value_ = value;
      validated = true;
   }
   else if (p->minimum_.has_value() && value < p->minimum_)
   {
      p->value_ = *p->minimum_;
   }
   else if (p->maximum_.has_value() && value > p->maximum_)
   {
      p->value_ = *p->maximum_;
   }
   else
   {
      p->value_ = p->default_;
   }

   return validated;
}

template<class T>
void SettingsVariable<T>::SetValueToDefault()
{
   p->value_ = p->default_;
}

template<class T>
bool SettingsVariable<T>::StageValue(const T& value)
{
   bool validated = false;

   if (p->Validate(value))
   {
      p->staged_ = value;
      validated  = true;
   }

   return validated;
}

template<class T>
void SettingsVariable<T>::Commit()
{
   if (p->staged_.has_value())
   {
      p->value_ = std::move(*p->staged_);
      p->staged_.reset();
   }
}

template<class T>
T SettingsVariable<T>::GetDefault() const
{
   return p->default_;
}

template<class T>
void SettingsVariable<T>::SetDefault(const T& value)
{
   p->default_ = value;
}

template<class T>
void SettingsVariable<T>::SetMinimum(const T& value)
{
   p->minimum_ = value;
}

template<class T>
void SettingsVariable<T>::SetMaximum(const T& value)
{
   p->maximum_ = value;
}

template<class T>
void SettingsVariable<T>::SetValidator(std::function<bool(const T&)> validator)
{
   p->validator_ = validator;
}

template<class T>
bool SettingsVariable<T>::Impl::Validate(const T& value)
{
   return ((!minimum_.has_value() || value >= minimum_) && // Validate minimum
           (!maximum_.has_value() || value <= maximum_) && // Validate maximum
           (validator_ == nullptr || validator_(value)));  // User-validation
}

} // namespace settings
} // namespace qt
} // namespace scwx
