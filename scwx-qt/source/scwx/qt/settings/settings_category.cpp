#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::settings_category";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class SettingsCategory::Impl
{
public:
   explicit Impl(const std::string& name) : name_ {name} {}

   ~Impl() {}

   const std::string name_;

   std::vector<SettingsVariableBase*> variables_;
};

SettingsCategory::SettingsCategory(const std::string& name) :
    p(std::make_unique<Impl>(name))
{
}
SettingsCategory::~SettingsCategory() = default;

SettingsCategory::SettingsCategory(SettingsCategory&&) noexcept = default;
SettingsCategory&
SettingsCategory::operator=(SettingsCategory&&) noexcept = default;

std::string SettingsCategory::name() const
{
   return p->name_;
}

void SettingsCategory::SetDefaults()
{
   for (auto& variable : p->variables_)
   {
      variable->SetValueToDefault();
   }
}

bool SettingsCategory::ReadJson(const boost::json::object& json)
{
   bool validated = true;

   const boost::json::value* value = json.if_contains(p->name_);

   if (value != nullptr && value->is_object())
   {
      const boost::json::object& object = value->as_object();

      for (auto& variable : p->variables_)
      {
         validated &= variable->ReadValue(object);
      }
   }
   else
   {
      if (value == nullptr)
      {
         logger_->warn("Key {} is not present, resetting to defaults",
                       p->name_);
      }
      else if (!value->is_object())
      {
         logger_->warn("Invalid json for key {}, resetting to defaults",
                       p->name_);
      }

      SetDefaults();
      validated = false;
   }

   return validated;
}

void SettingsCategory::WriteJson(boost::json::object& json) const
{
   boost::json::object object;

   for (auto& variable : p->variables_)
   {
      variable->WriteValue(object);
   }

   json.insert_or_assign(p->name_, object);
}

void SettingsCategory::RegisterVariables(
   std::initializer_list<SettingsVariableBase*> variables)
{
   p->variables_.insert(p->variables_.end(), variables);
}

void SettingsCategory::RegisterVariables(
   std::vector<SettingsVariableBase*> variables)
{
   p->variables_.insert(
      p->variables_.end(), variables.cbegin(), variables.cend());
}

} // namespace settings
} // namespace qt
} // namespace scwx
