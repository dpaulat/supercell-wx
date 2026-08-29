#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/util/logger.hpp>

#include <boost/json.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/uuid/random_generator.hpp>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

namespace scwx::qt::settings
{

static const std::string logPrefix_ = "scwx::qt::settings::settings_variable";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

template<class T>
class SettingsVariable<T>::Impl
{
public:
   explicit Impl()               = default;
   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   T                             value_ {};
   T                             default_ {};
   std::optional<T>              staged_ {};
   std::optional<T>              minimum_ {};
   std::optional<T>              maximum_ {};
   std::function<T(const T&)>    transform_ {};
   std::function<bool(const T&)> validator_ {nullptr};

   boost::signals2::signal<void(const ChangedEvent<T>&)> changedSignal_ {};
   boost::signals2::signal<void(const ChangedEvent<T>&)> stagedSignal_ {};

   boost::unordered_flat_map<boost::uuids::uuid, ValueCallbackFunction>
      valueChangedCallbackFunctions_ {};
   boost::unordered_flat_map<boost::uuids::uuid, ValueCallbackFunction>
      valueStagedCallbackFunctions_ {};
};

template<class T>
SettingsVariable<T>::SettingsVariable(const std::string& name) :
    SettingsVariableBase(name), p(std::make_unique<Impl>())
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
inline auto FormatParameter(const T& value)
{
   if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T> ||
                 std::is_same_v<T, std::string>)
   {
      return value;
   }
   else
   {
      return fmt::join(value, ", ");
   }
}

template<class T>
boost::signals2::signal<void(const ChangedEvent<T>&)>&
SettingsVariable<T>::changed_signal()
{
   return p->changedSignal_;
}

template<class T>
boost::signals2::signal<void(const ChangedEvent<T>&)>&
SettingsVariable<T>::staged_signal()
{
   return p->stagedSignal_;
}

template<class T>
bool SettingsVariable<T>::IsDefault() const
{
   return p->value_ == p->default_;
}

template<class T>
bool SettingsVariable<T>::IsDefaultStaged() const
{
   return p->staged_.value_or(p->value_) == p->default_;
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

   if (Validate(value))
   {
      const T oldValue        = p->value_;
      const T prevStagedValue = GetStagedOrValue();

      p->value_ = (p->transform_ != nullptr) ? p->transform_(value) : value;
      p->staged_.reset();
      validated = true;

      changed_signal()({oldValue, p->value_});
      for (auto& callback : p->valueChangedCallbackFunctions_)
      {
         callback.second(p->value_);
      }

      staged_signal()({prevStagedValue, p->value_});
      for (auto& callback : p->valueStagedCallbackFunctions_)
      {
         callback.second(p->value_);
      }
   }

   return validated;
}

template<class T>
bool SettingsVariable<T>::SetValueOrDefault(const T& value)
{
   bool validated = false;

   const T oldValue        = p->value_;
   const T prevStagedValue = GetStagedOrValue();

   if (Validate(value))
   {
      p->value_ = (p->transform_ != nullptr) ? p->transform_(value) : value;
      validated = true;
   }
   else if (p->minimum_.has_value() && value < p->minimum_)
   {
      logger_->warn("{0} less than minimum ({1} < {2}), setting to: {2}",
                    name(),
                    FormatParameter<T>(value),
                    FormatParameter<T>(*p->minimum_));
      p->value_ = *p->minimum_;
   }
   else if (p->maximum_.has_value() && value > p->maximum_)
   {
      logger_->warn("{0} greater than maximum ({1} > {2}), setting to: {2}",
                    name(),
                    FormatParameter<T>(value),
                    FormatParameter<T>(*p->maximum_));
      p->value_ = *p->maximum_;
   }
   else
   {
      logger_->warn("{} validation failed ({}), setting to default: {}",
                    name(),
                    FormatParameter<T>(value),
                    FormatParameter<T>(p->default_));
      p->value_ = p->default_;
   }

   p->staged_.reset();

   changed_signal()({oldValue, p->value_});
   for (auto& callback : p->valueChangedCallbackFunctions_)
   {
      callback.second(p->value_);
   }

   staged_signal()({prevStagedValue, p->value_});
   for (auto& callback : p->valueStagedCallbackFunctions_)
   {
      callback.second(p->value_);
   }

   return validated;
}

template<class T>
void SettingsVariable<T>::SetValueToDefault()
{
   const T oldValue        = p->value_;
   const T prevStagedValue = GetStagedOrValue();

   p->value_ = p->default_;
   p->staged_.reset();

   changed_signal()({oldValue, p->value_});
   for (auto& callback : p->valueChangedCallbackFunctions_)
   {
      callback.second(p->value_);
   }

   staged_signal()({prevStagedValue, p->value_});
   for (auto& callback : p->valueStagedCallbackFunctions_)
   {
      callback.second(p->value_);
   }
}

template<class T>
void SettingsVariable<T>::StageDefault()
{
   const T prevStagedValue = GetStagedOrValue();

   if (p->value_ != p->default_)
   {
      p->staged_ = p->default_;
   }
   else
   {
      p->staged_.reset();
   }

   staged_signal()({prevStagedValue, p->default_});
   for (auto& callback : p->valueStagedCallbackFunctions_)
   {
      callback.second(p->default_);
   }
}

template<class T>
bool SettingsVariable<T>::StageValue(const T& value)
{
   bool validated = false;

   if (Validate(value))
   {
      const T prevStagedValue = GetStagedOrValue();
      const T transformed =
         (p->transform_ != nullptr) ? p->transform_(value) : value;

      if (p->value_ != transformed)
      {
         p->staged_ = transformed;
      }
      else
      {
         p->staged_.reset();
      }

      validated = true;

      staged_signal()({prevStagedValue, transformed});
      for (auto& callback : p->valueStagedCallbackFunctions_)
      {
         callback.second(transformed);
      }
   }

   return validated;
}

template<class T>
bool SettingsVariable<T>::Commit()
{
   bool committed = false;

   if (p->staged_.has_value())
   {
      const T oldValue        = p->value_;
      const T prevStagedValue = GetStagedOrValue();

      p->value_ = std::move(*p->staged_);
      p->staged_.reset();
      committed = true;

      changed_signal()({oldValue, p->value_});
      for (auto& callback : p->valueChangedCallbackFunctions_)
      {
         callback.second(p->value_);
      }

      staged_signal()({prevStagedValue, p->value_});
      for (auto& callback : p->valueStagedCallbackFunctions_)
      {
         callback.second(p->value_);
      }
   }

   return committed;
}

template<class T>
void SettingsVariable<T>::Reset()
{
   const T prevStagedValue = GetStagedOrValue();

   p->staged_.reset();

   staged_signal()({prevStagedValue, p->value_});
   for (auto& callback : p->valueStagedCallbackFunctions_)
   {
      callback.second(p->value_);
   }
}

template<class T>
std::optional<T> SettingsVariable<T>::GetStaged() const
{
   return p->staged_;
}

template<class T>
T SettingsVariable<T>::GetStagedOrValue() const
{
   return p->staged_.value_or(GetValue());
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
std::optional<T> SettingsVariable<T>::GetMinimum() const
{
   return p->minimum_;
}

template<class T>
void SettingsVariable<T>::SetMaximum(const T& value)
{
   p->maximum_ = value;
}

template<class T>
std::optional<T> SettingsVariable<T>::GetMaximum() const
{
   return p->maximum_;
}

template<class T>
void SettingsVariable<T>::SetTransform(std::function<T(const T&)> transform)
{
   p->transform_ = transform;
}

template<class T>
void SettingsVariable<T>::SetValidator(std::function<bool(const T&)> validator)
{
   p->validator_ = validator;
}

template<class T>
bool SettingsVariable<T>::Validate(const T& value) const
{
   return (
      (!p->minimum_.has_value() || value >= p->minimum_) && // Validate minimum
      (!p->maximum_.has_value() || value <= p->maximum_) && // Validate maximum
      (p->validator_ == nullptr || p->validator_(value)));  // User-validation
}

template<class T>
bool SettingsVariable<T>::ReadValue(const boost::json::object& json)
{
   const boost::json::value* jv        = json.if_contains(name());
   bool                      validated = false;

   if (jv != nullptr)
   {
      try
      {
         // value_to() throws with_throw_location<system_error>, which Clang 20
         // + libc++ does not catch as std::exception.
         const auto converted = boost::json::try_value_to<T>(*jv);
         if (converted.has_error())
         {
            logger_->warn("{} is invalid ({}), setting to default: {}",
                          name(),
                          converted.error().message(),
                          FormatParameter<T>(p->default_));
            p->value_ = p->default_;
         }
         else
         {
            validated = SetValueOrDefault(*converted);
         }
      }
      catch (const std::exception& ex)
      {
         logger_->warn("{} is invalid ({}), setting to default: {}",
                       name(),
                       ex.what(),
                       FormatParameter<T>(p->default_));
         p->value_ = p->default_;
      }
      catch (...)
      {
         logger_->warn("{} is invalid, setting to default: {}",
                       name(),
                       FormatParameter<T>(p->default_));
         p->value_ = p->default_;
      }
   }
   else
   {
      logger_->debug("{} is not present, setting to default: {}",
                     name(),
                     FormatParameter<T>(p->default_));
      SetValueToDefault();
   }

   return validated;
}

template<class T>
void SettingsVariable<T>::WriteValue(boost::json::object& json) const
{
   json[name()] = boost::json::value_from<T&>(p->value_);
}

template<class T>
boost::signals2::connection
SettingsVariable<T>::ConnectChanged(std::function<void()> slot)
{
   return changed_signal().connect(
      [slot = std::move(slot)](const ChangedEvent<T>&) { slot(); });
}

template<class T>
boost::signals2::connection
SettingsVariable<T>::ConnectStaged(std::function<void()> slot)
{
   return staged_signal().connect(
      [slot = std::move(slot)](const ChangedEvent<T>&) { slot(); });
}

template<class T>
boost::uuids::uuid SettingsVariable<T>::RegisterValueChangedCallback(
   ValueCallbackFunction callback)
{
   boost::uuids::uuid uuid = boost::uuids::random_generator()();
   p->valueChangedCallbackFunctions_.emplace(uuid, std::move(callback));
   return uuid;
}

template<class T>
void SettingsVariable<T>::UnregisterValueChangedCallback(
   boost::uuids::uuid uuid)
{
   p->valueChangedCallbackFunctions_.erase(uuid);
}

template<class T>
boost::uuids::uuid
SettingsVariable<T>::RegisterValueStagedCallback(ValueCallbackFunction callback)
{
   boost::uuids::uuid uuid = boost::uuids::random_generator()();
   p->valueStagedCallbackFunctions_.emplace(uuid, std::move(callback));
   return uuid;
}

template<class T>
void SettingsVariable<T>::UnregisterValueStagedCallback(boost::uuids::uuid uuid)
{
   p->valueStagedCallbackFunctions_.erase(uuid);
}

template<class T>
bool SettingsVariable<T>::Equals(const SettingsVariableBase& o) const
{
   // This is only ever called with SettingsVariable<T>, so static_cast is safe
   const SettingsVariable<T>& v = static_cast<const SettingsVariable<T>&>(o);

   // Don't compare transform or validator
   return SettingsVariableBase::Equals(o) && //
          p->value_ == v.p->value_ &&        //
          p->default_ == v.p->default_ &&    //
          p->staged_ == v.p->staged_ &&      //
          p->minimum_ == v.p->minimum_ &&    //
          p->maximum_ == v.p->maximum_;
}

template class SettingsVariable<bool>;
template class SettingsVariable<double>;
template class SettingsVariable<std::int64_t>;
template class SettingsVariable<std::string>;

// Containers are not to be used directly
template class SettingsVariable<std::vector<std::int64_t>>;

} // namespace scwx::qt::settings
