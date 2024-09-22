#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>

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

   std::vector<std::pair<std::string, std::vector<SettingsCategory*>>>
                                      subcategoryArrays_;
   std::vector<SettingsCategory*>     subcategories_;
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
   // Set subcategory array defaults
   for (auto& subcategoryArray : p->subcategoryArrays_)
   {
      for (auto& subcategory : subcategoryArray.second)
      {
         subcategory->SetDefaults();
      }
   }

   // Set subcategory defaults
   for (auto& subcategory : p->subcategories_)
   {
      subcategory->SetDefaults();
   }

   // Set variable defaults
   for (auto& variable : p->variables_)
   {
      variable->SetValueToDefault();
   }
}

bool SettingsCategory::Commit()
{
   bool committed = false;

   // Commit subcategory arrays
   for (auto& subcategoryArray : p->subcategoryArrays_)
   {
      for (auto& subcategory : subcategoryArray.second)
      {
         committed |= subcategory->Commit();
      }
   }

   // Commit subcategories
   for (auto& subcategory : p->subcategories_)
   {
      committed |= subcategory->Commit();
   }

   // Commit variables
   for (auto& variable : p->variables_)
   {
      committed |= variable->Commit();
   }

   return committed;
}

bool SettingsCategory::ReadJson(const boost::json::object& json)
{
   bool validated = true;

   const boost::json::value* value = json.if_contains(p->name_);

   if (value != nullptr && value->is_object())
   {
      const boost::json::object& object = value->as_object();

      // Read subcategory arrays
      for (auto& subcategoryArray : p->subcategoryArrays_)
      {
         const boost::json::value* arrayValue =
            object.if_contains(subcategoryArray.first);

         if (arrayValue != nullptr && arrayValue->is_object())
         {
            const boost::json::object& arrayObject = arrayValue->as_object();

            for (auto& subcategory : subcategoryArray.second)
            {
               validated &= subcategory->ReadJson(arrayObject);
            }
         }
         else
         {
            if (arrayValue == nullptr)
            {
               logger_->debug(
                  "Subcategory array key {} is not present, resetting to "
                  "defaults",
                  subcategoryArray.first);
            }
            else if (!arrayValue->is_object())
            {
               logger_->warn(
                  "Invalid json for subcategory array key {}, resetting to "
                  "defaults",
                  p->name_);
            }

            for (auto& subcategory : subcategoryArray.second)
            {
               subcategory->SetDefaults();
            }
            validated = false;
         }
      }

      // Read subcategories
      for (auto& subcategory : p->subcategories_)
      {
         validated &= subcategory->ReadJson(object);
      }

      // Read variables
      for (auto& variable : p->variables_)
      {
         validated &= variable->ReadValue(object);
      }
   }
   else
   {
      if (value == nullptr)
      {
         logger_->debug("Key {} is not present, resetting to defaults",
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

   // Write subcategory arrays
   for (auto& subcategoryArray : p->subcategoryArrays_)
   {
      boost::json::object arrayObject;

      for (auto& subcategory : subcategoryArray.second)
      {
         subcategory->WriteJson(arrayObject);
      }

      object.insert_or_assign(subcategoryArray.first, arrayObject);
   }

   // Write subcategories
   for (auto& subcategory : p->subcategories_)
   {
      subcategory->WriteJson(object);
   }

   // Write variables
   for (auto& variable : p->variables_)
   {
      variable->WriteValue(object);
   }

   json.insert_or_assign(p->name_, object);
}

void SettingsCategory::RegisterSubcategory(SettingsCategory& subcategory)
{
   p->subcategories_.push_back(&subcategory);
}

void SettingsCategory::RegisterSubcategoryArray(
   const std::string& name, std::vector<SettingsCategory>& subcategories)
{
   auto& newSubcategories = p->subcategoryArrays_.emplace_back(
      name, std::vector<SettingsCategory*> {});

   std::transform(subcategories.begin(),
                  subcategories.end(),
                  std::back_inserter(newSubcategories.second),
                  [](SettingsCategory& subcategory) { return &subcategory; });
}

void SettingsCategory::RegisterSubcategoryArray(
   const std::string& name, std::vector<SettingsCategory*>& subcategories)
{
   auto& newSubcategories = p->subcategoryArrays_.emplace_back(
      name, std::vector<SettingsCategory*> {});

   std::transform(subcategories.begin(),
                  subcategories.end(),
                  std::back_inserter(newSubcategories.second),
                  [](SettingsCategory* subcategory) { return subcategory; });
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
