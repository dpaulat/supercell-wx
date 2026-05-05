#include <scwx/qt/settings/spc_outlook_settings.hpp>
#include <scwx/util/logger.hpp>

#include <boost/json.hpp>

namespace scwx::qt::settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::spc_outlook_settings";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static const std::string kSelectedDayName_     = "selected_day";
static const std::string kSelectedProductName_ = "selected_product";
static const std::string kOpacityName_         = "opacity";
static const std::string kAutoRefreshName_     = "auto_refresh";

class SpcOutlookSettings::Impl
{
public:
   Impl()
   {
      selectedDay_.SetDefault("Day 1");
      selectedProduct_.SetDefault("Categorical");
      opacity_.SetDefault(70);
      autoRefresh_.SetDefault(true);

      opacity_.SetValidator([](int value)
                            { return value >= 0 && value <= 100; });

      variables_ = {&selectedDay_, &selectedProduct_, &opacity_, &autoRefresh_};
   }

   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   SettingsVariable<std::string> selectedDay_ {kSelectedDayName_};
   SettingsVariable<std::string> selectedProduct_ {kSelectedProductName_};
   SettingsVariable<int>         opacity_ {kOpacityName_};
   SettingsVariable<bool>        autoRefresh_ {kAutoRefreshName_};

   std::vector<SettingsVariableBase*> variables_ {};
};

SpcOutlookSettings::SpcOutlookSettings() :
    SettingsCategory("spc_outlook"), p(std::make_unique<Impl>())
{
   RegisterVariables(p->variables_);
}
SpcOutlookSettings::~SpcOutlookSettings() = default;

SpcOutlookSettings::SpcOutlookSettings(SpcOutlookSettings&&) noexcept = default;
SpcOutlookSettings&
SpcOutlookSettings::operator=(SpcOutlookSettings&&) noexcept = default;

SettingsVariable<std::string>& SpcOutlookSettings::selected_day()
{
   return p->selectedDay_;
}
SettingsVariable<std::string>& SpcOutlookSettings::selected_product()
{
   return p->selectedProduct_;
}
SettingsVariable<int>& SpcOutlookSettings::opacity()
{
   return p->opacity_;
}
SettingsVariable<bool>& SpcOutlookSettings::auto_refresh()
{
   return p->autoRefresh_;
}

bool SpcOutlookSettings::ReadJson(const boost::json::object& json)
{
   bool validated = true;

   const boost::json::value* value = json.if_contains(name());
   if (value != nullptr && value->is_object())
   {
      const boost::json::object& obj = value->as_object();

      validated &= p->selectedDay_.ReadValue(obj);
      validated &= p->selectedProduct_.ReadValue(obj);
      validated &= p->opacity_.ReadValue(obj);
      validated &= p->autoRefresh_.ReadValue(obj);
   }
   else
   {
      if (value == nullptr)
      {
         logger_->warn("Key is not present, resetting to defaults");
      }
      else if (!value->is_object())
      {
         logger_->warn("Invalid json, resetting to defaults");
      }

      SetDefaults();
      validated = false;
   }

   return validated;
}

void SpcOutlookSettings::WriteJson(boost::json::object& json) const
{
   boost::json::object obj;

   p->selectedDay_.WriteValue(obj);
   p->selectedProduct_.WriteValue(obj);
   p->opacity_.WriteValue(obj);
   p->autoRefresh_.WriteValue(obj);

   json.insert_or_assign(name(), std::move(obj));
}

SpcOutlookSettings& SpcOutlookSettings::Instance()
{
   static SpcOutlookSettings instance_;
   return instance_;
}

} // namespace scwx::qt::settings
